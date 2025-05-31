#include "imgui.h"

#include "common.hpp"

#include <sstream>
#include <inttypes.h>

#include <fmt/format.h>

static int scroll_highlight_addr = -1;
static float scroll_highlight_time;
constexpr float HIGHLIGHT_DURATION = 2.f;

// defined in window_data_space
void hover_data_space(uint16_t addr);

static std::string prog_addr_name(uint16_t addr)
{
    if(addr / 4 < absim::INT_VECTOR_INFO.size())
    {
        auto const& info = absim::INT_VECTOR_INFO[addr / 4];
        if(info.name)
            return info.name;
    }
    auto const* sym = arduboy.symbol_for_prog_addr(addr);
    if(sym)
    {
        if(addr == sym->addr)
            return sym->name.c_str();
        if(arduboy.elf)
        {
            auto index = arduboy.elf->addr_to_disassembled_index(addr);
            auto const& a = arduboy.elf->asm_with_source[index];
            if(a.type == a.SYMBOL)
            {
                auto it = arduboy.elf->text_symbols.find(a.addr);
                if(it != arduboy.elf->text_symbols.end())
                    return it->second.name;
            }
        }
        return fmt::format("{} + {}", sym->name, addr - sym->addr);
    }
    return "";
}

static absim::disassembled_instr_t const& dis_instr(int row)
{
    size_t index = (size_t)row;
    if(arduboy.elf)
    {
        if(row < arduboy.elf->asm_with_source.size())
            return arduboy.elf->asm_with_source[row];
        index -= arduboy.elf->asm_with_source.size();
        index += arduboy.cpu.num_instrs;
    }
    if(index < arduboy.cpu.disassembled_prog.size())
        return arduboy.cpu.disassembled_prog[index];
    static const absim::disassembled_instr_t DUMMY =
    { nullptr, absim::disassembled_instr_t::OBJECT, 1 };
    return DUMMY;
}

static char const* get_prog_addr_source_line(uint16_t addr)
{
    if(!arduboy.elf)
        return nullptr;
    auto const& elf = *arduboy.elf;
    auto it = elf.source_lines.find(addr);
    if(it == elf.source_lines.end())
        return nullptr;
    int file = it->second.first;
    int line = it->second.second;
    if(file >= elf.source_files.size())
        return nullptr;
    auto const& sf = elf.source_files[file];
    if(line >= sf.lines.size())
        return nullptr;
    return sf.lines[line].c_str();
}

static std::string disassembly_arg_str(
    uint16_t addr,
    absim::disassembled_instr_arg_t const& a,
    bool diffable = false)
{
    switch(a.type)
    {
    case absim::disassembled_instr_arg_t::type::REG:
        return fmt::format("r{}", a.val);
    case absim::disassembled_instr_arg_t::type::IMM:
        return fmt::format("{:#04x}", a.val);
    case absim::disassembled_instr_arg_t::type::BIT:
        return fmt::format("{}", a.val);
    case absim::disassembled_instr_arg_t::type::DS_ADDR:
        return fmt::format("{:#06x}", a.val);
    case absim::disassembled_instr_arg_t::type::PROG_ADDR:
    {
        auto p = prog_addr_name(a.val * 2);
        if(!diffable || p.empty())
            return fmt::format("{:#06x} [{}]", a.val, p);
        return fmt::format("[{}]", p);
    }
    case absim::disassembled_instr_arg_t::type::OFFSET:
    {
        auto p = prog_addr_name(addr + (int16_t)a.val * 2);
        if(!diffable || p.empty())
            return fmt::format(".{:+d} [{}]", (int16_t)a.val * 2, p);
        return fmt::format("[{}]", p);
    }
    case absim::disassembled_instr_arg_t::type::PTR_REG:
        return fmt::format("{}", char('X' + (a.val - 26) / 2));
    case absim::disassembled_instr_arg_t::type::PTR_REG_OFFSET:
        return fmt::format("{}+{}", char('X' + ((a.val & 0xff) - 26) / 2), (a.val >> 8) & 0xff);
    case absim::disassembled_instr_arg_t::type::PTR_REG_PRE_DEC:
        return fmt::format("-{}", char('X' + (a.val - 26) / 2));
    case absim::disassembled_instr_arg_t::type::PTR_REG_POST_INC:
        return fmt::format("{}+", char('X' + (a.val - 26) / 2));
    case absim::disassembled_instr_arg_t::type::IO_REG:
        return fmt::format("{:#04x}", a.val);
    default:
        return "";
    }
}

static void copy_disassembly_to_clipboard(bool diffable = false)
{
    int num = arduboy.elf ?
        (int)arduboy.elf->asm_with_source.size() :
        (int)arduboy.cpu.num_instrs;
    std::ostringstream ss;
    for(int i = 0; i < num; ++i)
    {
        auto const& d = dis_instr(i);
        switch(d.type)
        {
        case absim::disassembled_instr_t::SOURCE:
            if(!diffable)
                ss << fmt::format("        ; {}\n", get_prog_addr_source_line(d.addr));
            break;
        case absim::disassembled_instr_t::SYMBOL:
        {
            if(!arduboy.elf)
                break;
            auto const& elf = *arduboy.elf;
            auto it = elf.text_symbols.find(d.addr);
            if(it == elf.text_symbols.end())
                break;
            if(!diffable)
                ss << "        ";
            ss << fmt::format("{}:\n", it->second.name);
            break;
        }
        case absim::disassembled_instr_t::OBJECT:
        {
            std::array<char, 3 * 8> buf;
            std::array<char, 9> charbuf;
            int i;
            for(i = 0; i < d.obj_bytes && i < 8; ++i)
            {
                uint8_t byte = arduboy.cpu.prog[d.addr + i];
                if(i > 0) buf[i * 3 - 1] = ' ';
                buf[i * 3 + 0] = "0123456789abcdef"[byte >> 4];
                buf[i * 3 + 1] = "0123456789abcdef"[byte & 15];
                buf[i * 3 + 2] = '\0';
                char c = '.';
                if(byte >= 32 && byte < 127)
                    c = (char)byte;
                charbuf[i] = c;
                charbuf[i + 1] = '\0';
            }
            if(!diffable)
                ss << fmt::format("{:#06x}: ", d.addr);
            ss << fmt::format("  {} {}\n", buf.data(), charbuf.data());
            break;
        }
        case absim::disassembled_instr_t::INSTR:
        {
            if(!diffable)
                ss << fmt::format("{:#06x}: ", d.addr);
            ss << fmt::format("  {}", d.name ? d.name : "???");
            if(d.arg0.type != absim::disassembled_instr_arg_t::type::NONE)
                ss << " " << disassembly_arg_str(d.addr, d.arg0, diffable);
            if(d.arg1.type != absim::disassembled_instr_arg_t::type::NONE)
                ss << ", " << disassembly_arg_str(d.addr, d.arg1, diffable);
            ss << "\n";
            break;
        }
        default:
            break;
        }
    }
    platform_set_clipboard_text(ss.str().c_str());
}

static void register_tooltip(int reg, bool pointer = false)
{
    if(reg < 0 || reg > 31) return;
    using namespace ImGui;
    BeginTooltip();
    if(pointer)
        Text("Pointer: r%d:r%d (%c)", reg, reg + 1, 'X' + (reg - 26) / 2);
    else
        Text("Register: r%d", reg);
    Separator();
    uint8_t x = arduboy.cpu.data[reg];
    if(reg % 2 == 0)
    {
        uint8_t y = arduboy.cpu.data[reg + 1];
        uint16_t w = x | (uint16_t(y) << 8);
        if(!pointer)
            Text("[Single]  0x%02x    %5d  %+5d", x, x, (int8_t)x);
        Text("[Pair]    0x%02x%02x  %5d  %+5d", y, x, w, (int16_t)w);
    }
    else
    {
        Text("0x%02x  %3d  %+3d", x, x, (int8_t)x);
    }
    if(pointer)
    {

    }
    EndTooltip();
}

static int find_index_of_addr(uint16_t addr)
{
    return arduboy.elf ?
        (int)arduboy.elf->addr_to_disassembled_index(addr) :
        (int)arduboy.cpu.addr_to_disassembled_index(addr);
}

static void prog_addr_symbol_line(uint16_t addr)
{
    using namespace ImGui;
    if(!arduboy.elf)
        return;
    auto const& elf = *arduboy.elf;
    auto it = elf.text_symbols.find(addr);
    if(it == elf.text_symbols.end())
        return;
    PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.f, 1.f));
    TextUnformatted(it->second.name.c_str());
    PopStyleColor();
}

static void prog_addr_source_line(uint16_t addr)
{
    using namespace ImGui;
    if(!arduboy.elf)
        return;
    auto const& elf = *arduboy.elf;
    auto it = elf.source_lines.find(addr);
    if(it == elf.source_lines.end())
        return;
    int file = it->second.first;
    int line = it->second.second;
    if(file >= elf.source_files.size())
        return;
    auto const& sf = elf.source_files[file];
    if(line >= sf.lines.size())
        return;
    TextDisabled("%s", sf.lines[line].c_str());
    if(IsItemHovered())
    {
        BeginTooltip();
        TextUnformatted(sf.filename.c_str());
        Separator();
        constexpr int N = 7;
        for(int i = line - N; i <= line + N; ++i)
        {
            if(i < 0) continue;
            if(i >= sf.lines.size()) continue;
            if(i == line) PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.f, 1.f));
            Text("%5d  ", i);
            SameLine();
            TextUnformatted(sf.lines[i].c_str());
            if(i == line) PopStyleColor();
        }
        EndTooltip();
    }
}

static void prog_addr_tooltip(uint16_t addr)
{
    using namespace ImGui;
    if(addr / 4 < absim::INT_VECTOR_INFO.size())
    {
        auto const& info = absim::INT_VECTOR_INFO[addr / 4];
        if(info.name && info.desc)
        {
            BeginTooltip();
            TextUnformatted(info.name);
            Separator();
            TextUnformatted(info.desc);
            EndTooltip();
            return;
        }
    }
    auto name = prog_addr_name(addr);
    if(!name.empty())
    {
        BeginTooltip();
        TextUnformatted(name.c_str());
        EndTooltip();
    }
    //BeginTooltip();
    //prog_addr_source_line(addr);
    //EndTooltip();
}

static void disassembly_prog_addr(uint16_t addr, int& do_scroll)
{
    using namespace ImGui;
    auto name = prog_addr_name(addr);
    auto addr_size = CalcTextSize(name.empty() ? "0x0000" : name.c_str());
    auto saved = GetCursorPos();
    Dummy(addr_size);
    ImU32 color = IM_COL32(30, 200, 100, 255);
    if(IsItemHovered())
    {
        color = IM_COL32(80, 255, 150, 255);
        SetMouseCursor(ImGuiMouseCursor_Hand);
        prog_addr_tooltip(addr);
    }
    SetCursorPos(saved);
    PushStyleColor(ImGuiCol_Text, color);
    if(!name.empty())
        TextUnformatted(name.c_str());
    else
        Text("0x%04x", addr);
    PopStyleColor();
    if(IsItemClicked())
    {
        scroll_addr_to_top = true;
        do_scroll = addr;
    }
}

static void disassembly_arg(
    absim::disassembled_instr_t const& d,
    absim::disassembled_instr_arg_t const& a,
    int row, int& do_scroll)
{
    constexpr auto REGISTER_COLOR = IM_COL32(50, 120, 230, 255);
    constexpr auto IO_COLOR = IM_COL32(230, 120, 50, 255);
    using namespace ImGui;
    switch(a.type)
    {
    case absim::disassembled_instr_arg_t::type::REG:
        PushStyleColor(ImGuiCol_Text, REGISTER_COLOR);
        Text("r%d", a.val);
        PopStyleColor();
        if(IsItemHovered())
            register_tooltip(a.val);
        break;
    case absim::disassembled_instr_arg_t::type::IMM:
        Text("0x%02x", a.val);
        break;
    case absim::disassembled_instr_arg_t::type::BIT:
        Text("%d", a.val);
        break;
    case absim::disassembled_instr_arg_t::type::DS_ADDR:
    {
        if(a.val >= 32 && a.val < 0x100)
            PushStyleColor(ImGuiCol_Text, IO_COLOR);
        Text("0x%04x", a.val);
        if(a.val >= 32 && a.val < 0x100)
            PopStyleColor();
        if(IsItemHovered())
        {
            hover_data_space(a.val);
            //auto const* sym = arduboy.symbol_for_data_addr(a.val);
            //BeginTooltip();
            //if(sym)
            //{
            //    if(sym->size > 1)
            //        Text("%s [byte %d]", sym->name.c_str(), int(a.val - sym->addr));
            //    else
            //        TextUnformatted(sym->name.c_str());
            //}
            //else
            //{
            //    Text("Data Space: 0x%04x", a.val);
            //}
            //Separator();
            //if(a.val < arduboy.cpu.data.size())
            //{
            //    uint8_t x = arduboy.cpu.data[a.val];
            //    Text("0x%02x  %d  %d", x, x, (int8_t)x);
            //}
            //EndTooltip();
        }
        break;
    }
    case absim::disassembled_instr_arg_t::type::PROG_ADDR:
    {
        disassembly_prog_addr(a.val * 2, do_scroll);
        break;
    }
    case absim::disassembled_instr_arg_t::type::OFFSET:
    {
        Text(".%+d", (int16_t)a.val * 2);
        SameLine();
        TextUnformatted("[");
        SameLine(0.f, 0.f);
        disassembly_prog_addr(d.addr + 2 + (int16_t)a.val * 2, do_scroll);
        SameLine(0.f, 0.f);
        TextUnformatted("]");
        break;
    }
    case absim::disassembled_instr_arg_t::type::PTR_REG:
    {
        PushStyleColor(ImGuiCol_Text, REGISTER_COLOR);
        Text("%c", 'X' + (a.val - 26) / 2);
        PopStyleColor();
        if(IsItemHovered())
            register_tooltip(a.val, true);
        break;
    }
    case absim::disassembled_instr_arg_t::type::PTR_REG_OFFSET:
    {
        uint8_t reg = (a.val >> 0) & 0xff;
        uint8_t off = (a.val >> 8) & 0xff;
        PushStyleColor(ImGuiCol_Text, REGISTER_COLOR);
        Text("%c", 'X' + (reg - 26) / 2);
        PopStyleColor();
        if(IsItemHovered())
            register_tooltip(reg, true);
        SameLine(0.f, 0.f);
        Text("+%d", off);
        break;
    }
    case absim::disassembled_instr_arg_t::type::PTR_REG_PRE_DEC:
    {
        PushStyleColor(ImGuiCol_Text, REGISTER_COLOR);
        TextUnformatted("-");
        SameLine(0.f, 0.f);
        Text("%c", 'X' + (a.val - 26) / 2);
        PopStyleColor();
        if(IsItemHovered())
            register_tooltip(a.val, true);
        break;
    }
    case absim::disassembled_instr_arg_t::type::PTR_REG_POST_INC:
    {
        PushStyleColor(ImGuiCol_Text, REGISTER_COLOR);
        Text("%c", 'X' + (a.val - 26) / 2);
        PopStyleColor();
        if(IsItemHovered())
            register_tooltip(a.val, true);
        SameLine(0.f, 0.f);
        TextUnformatted("+");
        break;
    }
    case absim::disassembled_instr_arg_t::type::IO_REG:
    {
        PushStyleColor(ImGuiCol_Text, IO_COLOR);
        Text("0x%02x", a.val);
        PopStyleColor();
        if(IsItemHovered())
            hover_data_space(a.val + 32);
    }
    default:
        break;
    }
}

static void toggle_breakpoint(int row)
{
    auto addr = dis_instr(row).addr;
    arduboy.breakpoints.flip(addr / 2);
}

static void draw_breakpoint_color(ImVec2 p, ImU32 color)
{
    using namespace ImGui;
    float const R = GetTextLineHeight() * 0.5f;
    p.x += R;
    p.y += R;
    auto* drawlist = GetWindowDrawList();
    drawlist->AddCircleFilled(p, R, color);
}

static void draw_breakpoint_hovered(int row, ImVec2 p)
{
    constexpr auto BP_COLOR_HOVERED = IM_COL32(150, 40, 40, 255);
    auto addr = dis_instr(row).addr / 2;
    bool bp = arduboy.breakpoints.test(addr);
    if(!bp) draw_breakpoint_color(p, BP_COLOR_HOVERED);
}

static void draw_breakpoint(int row, ImVec2 p)
{
    constexpr auto BP_COLOR = IM_COL32(255, 40, 40, 255);
    auto addr = dis_instr(row).addr / 2;
    bool bp = arduboy.breakpoints.test(addr);
    if(bp) draw_breakpoint_color(p, BP_COLOR);
}

static void profiler_count(absim::disassembled_instr_t const& d)
{
    using namespace ImGui;
    uint64_t profiler_total = arduboy.profiler_enabled ?
        arduboy.profiler_total : arduboy.cached_profiler_total;
    if(profiler_total == 0) return;
    uint64_t count = arduboy.profiler_counts[d.addr / 2];
    if(count == 0) return;
    double f = double(count) * 100 / profiler_total;
    if(settings.profiler_cycle_counts)
        Text("%12" PRIu64 "%8.4f%%", count, f);
    else
        Text("%8.4f%%", f);
}

static char jump_buf[5];

static int hex_value(char c)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

void window_disassembly(bool& open)
{
    using namespace ImGui;
    if(!open) return;
    SetNextWindowSize({ 200 * pixel_ratio, 400 * pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Disassembly", &open) && arduboy.cpu.decoded)
    {
        int do_scroll = disassembly_scroll_addr;
        disassembly_scroll_addr = -1;
        if(scroll_highlight_time > 0)
            scroll_highlight_time -= GetIO().DeltaTime;
        AlignTextToFramePadding();
        TextUnformatted("Jump: 0x");
        SameLine(0.f, 0.f);
        SetNextItemWidth(
            CalcTextSize("FFFF").x +
            GetStyle().FramePadding.x * 2);
        if(InputText(
            "##jump",
            jump_buf, sizeof(jump_buf),
            ImGuiInputTextFlags_CharsHexadecimal |
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll))
        {
            do_scroll = 0;
            char* b = jump_buf;
            while(*b != 0)
                do_scroll = (do_scroll << 4) + hex_value(*b++);
        }
        SameLine();
        if(Button("PC"))
        {
            do_scroll = arduboy.cpu.pc * 2;
        }
        SameLine();
        if(Button("Copy"))
        {
            copy_disassembly_to_clipboard(
                IsKeyDown(ImGuiKey_LeftShift) || IsKeyDown(ImGuiKey_RightShift));
        }
        if(IsItemHovered() && BeginTooltip())
        {
            TextUnformatted("Click to copy disassembly to clipboard.");
            TextUnformatted("Shift-click to copy diffable disassembly.");
            EndTooltip();
        }
        

        static bool show_full_range = false;
        SameLine();
        Checkbox("Full", &show_full_range);

        if(arduboy.elf)
        {
            SameLine();
            SetNextItemWidth(GetContentRegionAvail().x);
            if(BeginCombo("##symboljump", "Jump to function...", ImGuiComboFlags_HeightLarge))
            {
                for(uint16_t addr : arduboy.elf->text_symbols_sorted)
                {
                    auto const& sym = arduboy.elf->text_symbols[addr];
                    if(sym.object || sym.weak || sym.notype) continue;
                    if(Selectable(sym.name.c_str()))
                        do_scroll = addr, scroll_addr_to_top = true;
                }
                EndCombo();
            }
        }

        ImGuiTableFlags flags = 0;
        flags |= ImGuiTableFlags_ScrollY;
        flags |= ImGuiTableFlags_RowBg;
        flags |= ImGuiTableFlags_NoClip;
        //flags |= ImGuiTableFlags_SizingFixedFit;
        auto const size = GetContentRegionAvail();
        float const cw = arduboy.elf ? 20.f * pixel_ratio : 0.f;
        if(BeginTable("##ScrollingRegion", 3, flags, {size.x - cw, 0.f}))
        {
            TableSetupColumn("Address",
                ImGuiTableColumnFlags_WidthFixed,
                CalcTextSize("   0x0000").x);
            TableSetupColumn("Instruction",
                ImGuiTableColumnFlags_WidthStretch);
            TableSetupColumn("Profiling",
                ImGuiTableColumnFlags_WidthFixed,
                CalcTextSize(settings.profiler_cycle_counts ? "000000000000100.0000%" : "100.0000%").x + 2.f);
            ImGuiListClipper clipper;
            int extra_instrs = (int)(arduboy.cpu.num_instrs_total - arduboy.cpu.num_instrs);
            clipper.Begin(
                show_full_range ?
                arduboy.elf ?
                (int)arduboy.elf->asm_with_source.size() + extra_instrs :
                (int)arduboy.cpu.num_instrs_total :
                arduboy.elf ?
                (int)arduboy.elf->asm_with_source.size() :
                (int)arduboy.cpu.num_instrs,
                GetTextLineHeightWithSpacing());
            while(clipper.Step())
            {
                for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {
                    auto const& d = dis_instr(i);
                    TableNextRow();
                    TableSetColumnIndex(0);
                    if(d.type == absim::disassembled_instr_t::SOURCE)
                    {
                        prog_addr_source_line(d.addr);
                        continue;
                    }
                    else if(d.type == absim::disassembled_instr_t::SYMBOL)
                    {
                        prog_addr_symbol_line(d.addr);
                        continue;
                    }
                    uint16_t instr_index = d.addr / 2;
                    if(d.addr == scroll_highlight_addr && scroll_highlight_time >= 0.f)
                    {
                        float f = scroll_highlight_time / HIGHLIGHT_DURATION;
                        auto color = IM_COL32(80, 80, 80, uint8_t(f * 255));
                        TableSetBgColor(ImGuiTableBgTarget_RowBg1, color);
                    }
                    if(instr_index == arduboy.cpu.pc)
                    {
                        auto color = arduboy.paused ?
                            IM_COL32(80, 0, 0, 255) :
                            IM_COL32(60, 60, 60, 255);
                        TableSetBgColor(ImGuiTableBgTarget_RowBg0, color);
                    }
                    else if(profiler_selected_hotspot >= 0)
                    {
                        auto color = i % 2 ?
                            IM_COL32(65, 0, 0, 255) :
                            IM_COL32(50, 0, 0, 255);
                        auto const& h = arduboy.profiler_hotspots[profiler_selected_hotspot];
                        uint16_t instr_begin = arduboy.cpu.disassembled_prog[h.begin].addr / 2;
                        uint16_t instr_end = arduboy.cpu.disassembled_prog[h.end].addr / 2;
                        if(instr_index >= instr_begin && instr_index <= instr_end)
                            TableSetBgColor(ImGuiTableBgTarget_RowBg0, color);
                    }
                    auto bp_pos = GetCursorScreenPos();
                    draw_breakpoint(i, bp_pos);
                    TextDisabled("   0x%04x", d.addr);
                    if(IsItemHovered())
                    {
                        SetMouseCursor(ImGuiMouseCursor_Hand);
                        TableSetColumnIndex(0);
                        draw_breakpoint_hovered(i, bp_pos);
                        prog_addr_tooltip(d.addr);
                    }
                    if(IsItemClicked())
                    {
                        toggle_breakpoint(i);
                    }
                    TableSetColumnIndex(1);
                    if(d.type == absim::disassembled_instr_t::OBJECT)
                    {
                        std::array<char, 3 * 8> buf;
                        std::array<char, 9> charbuf;
                        int i;
                        for(i = 0; i < d.obj_bytes && i < 8; ++i)
                        {
                            uint8_t byte = arduboy.cpu.prog[d.addr + i];
                            if(i > 0) buf[i * 3 - 1] = ' ';
                            buf[i * 3 + 0] = "0123456789abcdef"[byte >> 4];
                            buf[i * 3 + 1] = "0123456789abcdef"[byte & 15];
                            buf[i * 3 + 2] = '\0';
                            char c = '.';
                            if(byte >= 32 && byte < 127)
                                c = (char)byte;
                            charbuf[i] = c;
                            charbuf[i + 1] = '\0';
                        }
                        TextDisabled("  %-26s %s", buf.data(), charbuf.data());
                    }
                    else
                    {
                        if(d.name)
                            TextUnformatted(d.name);
                        else
                            TextDisabled("???");
                        if(d.arg0.type != absim::disassembled_instr_arg_t::type::NONE)
                        {
                            SameLine();
                            disassembly_arg(d, d.arg0, i, do_scroll);
                        }
                        if(d.arg1.type != absim::disassembled_instr_arg_t::type::NONE)
                        {
                            SameLine(0.f, 0.f);
                            TextUnformatted(",");
                            SameLine();
                            disassembly_arg(d, d.arg1, i, do_scroll);
                        }
                    }

                    TableSetColumnIndex(2);
                    profiler_count(d);
                }
            }

            if(do_scroll >= 0)
            {
                // TODO: don't scroll unless necessary?
                int index = find_index_of_addr(do_scroll);
                int num_display = clipper.DisplayEnd - clipper.DisplayStart;
                if(!scroll_addr_to_top) index -= num_display / 2;
                SetScrollY(clipper.ItemsHeight * index);
                scroll_addr_to_top = false;
                scroll_highlight_addr = do_scroll;
                scroll_highlight_time = HIGHLIGHT_DURATION;
            }

            EndTable();
        }

        if(arduboy.elf)
        {
            SameLine(0.f, 0.f);
            auto* draw = GetWindowDrawList();
            auto const cp = GetCursorScreenPos();

            uint32_t last_addr = show_full_range ? 0x7fff : arduboy.cpu.last_addr;
            float iend = size.y / float(last_addr + 1);
            size_t index = 0;
            auto const mp = GetMousePos();
            for(auto const& kv : arduboy.elf->text_symbols)
            {
                auto const& sym = kv.second;
                if(sym.size <= 0) continue;
                if(sym.weak) continue;
                float a = iend * sym.addr;
                float b = iend * std::min<uint32_t>(last_addr, sym.addr + sym.size);
                ImVec2 ra = { cp.x, cp.y + a };
                ImVec2 rb = { cp.x + cw, cp.y + b };
                draw->AddRectFilled(ra, rb, color_for_index(index++));
                if(mp.x >= ra.x && mp.y >= ra.y && mp.x < rb.x && mp.y < rb.y)
                {
                    BeginTooltip();
                    TextUnformatted(sym.name.c_str());
                    EndTooltip();
                    SetMouseCursor(ImGuiMouseCursor_Hand);
                    if(IsMouseClicked(0))
                    {
                        disassembly_scroll_addr = sym.addr;
                        scroll_addr_to_top = true;
                    }
                }
            }

            draw->AddRect(
                cp,
                { cp.x + cw, cp.y + size.y },
                IM_COL32(150, 150, 150, 255));
        }

    }
    End();
}
