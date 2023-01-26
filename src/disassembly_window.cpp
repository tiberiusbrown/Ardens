#include "imgui.h"

#include "absim.hpp"

extern absim::arduboy_t arduboy;

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
    absim::disassembled_instr_t temp;
    temp.addr = addr;
    auto it = std::lower_bound(
        arduboy.cpu.disassembled_prog.begin(),
        arduboy.cpu.disassembled_prog.begin() + arduboy.cpu.num_instrs,
        temp,
        [](auto const& a, auto const& b) { return a.addr < b.addr; }
    );

    auto index = std::distance(arduboy.cpu.disassembled_prog.begin(), it);

    return (int)index;
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
    }
    SetCursorPos(saved);
    PushStyleColor(ImGuiCol_Text, color);
    Text("0x%04x", addr);
    PopStyleColor();
    if(IsItemClicked())
        do_scroll = find_index_of_addr(addr);
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
            BeginTooltip();
            Text("Data Space: 0x%04x", a.val);
            Separator();
            if(a.val < arduboy.cpu.data.size())
            {
                uint8_t x = arduboy.cpu.data[a.val];
                Text("0x%02x  %d  %d", x, x, (int8_t)x);
            }
            EndTooltip();
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
    }
    default:
        break;
    }
}

static void toggle_breakpoint(int row)
{
    auto addr = arduboy.cpu.disassembled_prog[row].addr / 2;
    arduboy.cpu.breakpoints.flip(addr);
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
    auto addr = arduboy.cpu.disassembled_prog[row].addr / 2;
    bool bp = arduboy.cpu.breakpoints.test(addr);
    if(!bp) draw_breakpoint_color(p, BP_COLOR_HOVERED);
}

static void draw_breakpoint(int row, ImVec2 p)
{
    constexpr auto BP_COLOR = IM_COL32(255, 40, 40, 255);
    auto addr = arduboy.cpu.disassembled_prog[row].addr / 2;
    bool bp = arduboy.cpu.breakpoints.test(addr);
    if(bp) draw_breakpoint_color(p, BP_COLOR);
}

void disassembly_window()
{
    using namespace ImGui;
    if(Begin("Disassembly") && arduboy.cpu.decoded)
    {
        int do_scroll = -1;

        if(Button("Reset"))
        {
            arduboy.paused = false;
            arduboy.reset();
        }
        SameLine();
        if(arduboy.paused)
        {
            if(Button("Continue"))
                arduboy.paused = false;
            SameLine();
            if(Button("Step"))
            {
                arduboy.advance_instr();
                do_scroll = find_index_of_addr(arduboy.cpu.pc * 2);
            }
        }
        else
        {
            if(Button("Pause"))
            {
                arduboy.paused = true;
                while(arduboy.cpu.cycles_till_next_instr != 0)
                    arduboy.cpu.advance_cycle();
                do_scroll = find_index_of_addr(arduboy.cpu.pc * 2);
            }
        }

        ImGuiTableFlags flags = 0;
        flags |= ImGuiTableFlags_ScrollY;
        flags |= ImGuiTableFlags_RowBg;
        flags |= ImGuiTableFlags_SizingFixedFit;
        if(BeginTable("##ScrollingRegion", 2, flags))
        {
            TableSetupColumn("Address", 0, 60.f);
            TableSetupColumn("Instruction");
            ImGuiListClipper clipper;
            clipper.Begin((int)arduboy.cpu.num_instrs);
            while(clipper.Step())
            {
                for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {
                    auto const& d = arduboy.cpu.disassembled_prog[i];
                    TableNextRow();
                    if(d.addr / 2 == arduboy.cpu.pc)
                    {
                        auto color = arduboy.paused ?
                            IM_COL32(80, 0, 0, 255) :
                            IM_COL32(60, 60, 60, 255);
                        TableSetBgColor(ImGuiTableBgTarget_RowBg1, color);
                    }
                    TableSetColumnIndex(0);
                    auto bp_pos = GetCursorScreenPos();
                    draw_breakpoint(i, bp_pos);
                    TextDisabled("   0x%04x", d.addr);
                    if(IsItemHovered())
                    {
                        SetMouseCursor(ImGuiMouseCursor_Hand);
                        TableSetColumnIndex(0);
                        draw_breakpoint_hovered(i, bp_pos);
                    }
                    if(IsItemClicked())
                    {
                        toggle_breakpoint(i);
                    }
                    TableSetColumnIndex(1);
                    if(!d.name)
                    {
                        TextDisabled("%02x%02x",
                            arduboy.cpu.prog[i * 2 + 1],
                            arduboy.cpu.prog[i * 2 + 0]);
                    }
                    else
                    {
                        TextUnformatted(d.name);
                    }
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
            }

            if(do_scroll >= 0)
            {
                int num_display = clipper.DisplayEnd - clipper.DisplayStart;
                SetScrollY(clipper.ItemsHeight * (do_scroll - num_display / 2));
            }

            EndTable();
        }
    }
    End();
}
