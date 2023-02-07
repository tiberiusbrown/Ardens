#include "imgui.h"

#include "absim.hpp"

#include <inttypes.h>

extern absim::arduboy_t arduboy;
extern int profiler_selected_hotspot;
extern int disassembly_scroll_addr;
extern bool profiler_cycle_counts;

static bool scroll_addr_to_top;

// defined in window_data_space
void hover_data_space(uint16_t addr);

static absim::disassembled_instr_t const& dis_instr(int row)
{
    return arduboy.elf ?
        arduboy.elf->asm_with_source[row] :
        arduboy.cpu.disassembled_prog[row];
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
            if(i > sf.lines.size()) continue;
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
    auto const* sym = arduboy.symbol_for_prog_addr(addr);
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
    if(sym)
    {
        BeginTooltip();
        if(addr == sym->addr)
            TextUnformatted(sym->name.c_str());
        else
            Text("%s [%+d]", sym->name.c_str(), addr - sym->addr);
        EndTooltip();
    }
    //BeginTooltip();
    //prog_addr_source_line(addr);
    //EndTooltip();
}

static void disassembly_prog_addr(uint16_t addr, int& do_scroll)
{
    using namespace ImGui;
    auto addr_size = CalcTextSize("0x0000");
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
        Text("0x%04x", a.val);
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
            hover_data_space(a.val);
    }
    default:
        break;
    }
}

static void toggle_breakpoint(int row)
{
    auto addr = dis_instr(row).addr;
    arduboy.cpu.breakpoints.flip(addr / 2);
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
    bool bp = arduboy.cpu.breakpoints.test(addr);
    if(!bp) draw_breakpoint_color(p, BP_COLOR_HOVERED);
}

static void draw_breakpoint(int row, ImVec2 p)
{
    constexpr auto BP_COLOR = IM_COL32(255, 40, 40, 255);
    auto addr = dis_instr(row).addr / 2;
    bool bp = arduboy.cpu.breakpoints.test(addr);
    if(bp) draw_breakpoint_color(p, BP_COLOR);
}

static void profiler_count(absim::disassembled_instr_t const& d)
{
    using namespace ImGui;
    if(arduboy.profiler_total == 0) return;
    uint64_t count = arduboy.profiler_counts[d.addr / 2];
    if(count == 0) return;
    double f = double(count) * 100 / arduboy.profiler_total;
    if(profiler_cycle_counts)
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
    SetNextWindowSize({ 200, 400 }, ImGuiCond_FirstUseEver);
    int do_scroll = disassembly_scroll_addr;
    disassembly_scroll_addr = -1;
    if(Begin("Disassembly", &open) && arduboy.cpu.decoded)
    {
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
        if(Button("Jump to PC"))
        {
            do_scroll = arduboy.cpu.pc * 2;
        }

        ImGuiTableFlags flags = 0;
        flags |= ImGuiTableFlags_ScrollY;
        flags |= ImGuiTableFlags_RowBg;
        flags |= ImGuiTableFlags_NoClip;
        //flags |= ImGuiTableFlags_SizingFixedFit;
        if(BeginTable("##ScrollingRegion", 3, flags))
        {
            TableSetupColumn("Address",
                ImGuiTableColumnFlags_WidthFixed,
                CalcTextSize("   0x0000").x);
            TableSetupColumn("Instruction",
                ImGuiTableColumnFlags_WidthStretch);
            TableSetupColumn("Profiling",
                ImGuiTableColumnFlags_WidthFixed,
                CalcTextSize(profiler_cycle_counts ? "000000000000100.0000%" : "100.0000%").x + 2.f);
            ImGuiListClipper clipper;
            clipper.Begin(arduboy.elf ?
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
                    if(instr_index == arduboy.cpu.pc)
                    {
                        auto color = arduboy.paused ?
                            IM_COL32(80, 0, 0, 255) :
                            IM_COL32(60, 60, 60, 255);
                        TableSetBgColor(ImGuiTableBgTarget_RowBg1, color);
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
                            TableSetBgColor(ImGuiTableBgTarget_RowBg1, color);
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
                        TextDisabled("%02x %02x",
                            arduboy.cpu.prog[d.addr + 0],
                            arduboy.cpu.prog[d.addr + 1]);
                    }
                    else
                    {
                        TextUnformatted(d.name);
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
            }

            EndTable();
        }
    }
    End();
}
