#include "imgui.h"
#include "imgui_memory_editor.h"

#include "absim.hpp"

extern absim::arduboy_t arduboy;
static MemoryEditor memed_data_space;

static void draw_memory_breakpoints(size_t addr)
{
    using namespace ImGui;
    bool rd = false;
    bool wr = false;
    if(addr < arduboy.cpu.data.size())
    {
        rd = arduboy.breakpoints_rd.test(addr);
        wr = arduboy.breakpoints_wr.test(addr);
    }
    BeginDisabled(addr < 32 || addr >= arduboy.cpu.data.size());
    AlignTextToFramePadding();
    TextUnformatted("Break on:");
    SameLine();
    Checkbox("Read", &rd);
    SameLine();
    Checkbox("Write", &wr);
    EndDisabled();
    if(addr < arduboy.cpu.data.size())
    {
        arduboy.breakpoints_rd[addr] = rd;
        arduboy.breakpoints_wr[addr] = wr;
    }
}

static bool highlight_func(ImU8 const* data, size_t off, ImU32& color)
{
    bool r = false;
    if(off < 0x20)
    {
        color = IM_COL32(20, 20, 50, 255);
        r = true;
    }
    else if(off < 0x100)
    {
        color = absim::REG_INFO[off].name ?
            IM_COL32(50, 50, 20, 255) :
            IM_COL32(0, 0, 0, 255);
        r = true;
    }
    else if(off >= arduboy.cpu.min_stack)
    {
        color = IM_COL32(70, 0, 90, 255);
        r = true;
    }
    else if(arduboy.cpu.stack_check > 0x100 && off >= arduboy.cpu.stack_check)
    {
        color = IM_COL32(45, 45, 45, 255);
        r = true;
    }
    if(off < arduboy.cpu.data.size() && (
        arduboy.breakpoints_rd.test(off) ||
        arduboy.breakpoints_wr.test(off)))
    {
        color = IM_COL32(100, 50, 50, 255);
        r = true;
    }
    return r;
}

void hover_data_space(uint16_t addr)
{
    using namespace ImGui;
    auto const* sym = arduboy.symbol_for_data_addr(addr);
    BeginTooltip();
    if(addr < 256)
    {
        auto const& r = absim::REG_INFO[addr];
        if(r.name)
        {
            constexpr auto UNUSED = IM_COL32(50, 50, 50, 255);
            constexpr auto BIT_SET = IM_COL32(30, 30, 100, 255);
            Text("0x%02x: %s", addr, r.name);
            if(addr >= 32)
            {
                Separator();
                Dummy({ 1.f, 4.f });
                if(BeginTable("##bits", 8,
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_SizingStretchSame))
                {
                    TableNextRow();
                    for(int i = 0; i < 8; ++i)
                    {
                        TableSetColumnIndex(i);
                        char const* bit = r.bits[7 - i];
                        if(bit)
                        {
                            uint8_t mask = 0x80 >> i;
                            if(arduboy.cpu.data[addr] & mask)
                                TableSetBgColor(ImGuiTableBgTarget_CellBg, BIT_SET);
                            TextUnformatted(bit);
                        }
                        else
                            TableSetBgColor(ImGuiTableBgTarget_CellBg, UNUSED);
                    }
                    EndTable();
                }
            }
        }
        else
            Text("0x%02x: Reserved", addr);
    }
    else if(sym)
    {
        if(sym->size > 1)
            Text("0x%04x: %s [byte %d]", addr, sym->name.c_str(), int(addr - sym->addr));
        else
            Text("0x%04x: %s", addr, sym->name.c_str());
    }
    else
    {
        Text("0x%04x", addr);
    }
    EndTooltip();
}

static void hover_func(ImU8 const* data, size_t off)
{
    (void)data;
    hover_data_space((uint16_t)off);
}

void window_data_space(bool& open)
{
    using namespace ImGui;

    {
        static bool first = true;
        if(first)
        {
            memed_data_space.OptShowDataPreview = true;
            memed_data_space.PreviewDataType = ImGuiDataType_U8;
            memed_data_space.OptFooterExtraHeight = GetFrameHeightWithSpacing();
            memed_data_space.HighlightFn = highlight_func;
            memed_data_space.HoverFn = hover_func;
            first = false;
        }
    }

    if(open)
    {
        SetNextWindowSize({ 200, 400 }, ImGuiCond_FirstUseEver);
        if(Begin("CPU Data Space", &open) && arduboy.cpu.decoded)
        {
            memed_data_space.DrawContents(
                arduboy.cpu.data.data(),
                arduboy.cpu.data.size());

            auto addr = memed_data_space.DataPreviewAddr;
            draw_memory_breakpoints(addr);
        }
        End();
    }
}
