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
        rd = arduboy.cpu.breakpoints_rd.test(addr);
        wr = arduboy.cpu.breakpoints_wr.test(addr);
    }
    BeginDisabled(addr >= arduboy.cpu.data.size());
    AlignTextToFramePadding();
    TextUnformatted("Break on:");
    SameLine();
    Checkbox("Read", &rd);
    SameLine();
    Checkbox("Write", &wr);
    EndDisabled();
    if(addr < arduboy.cpu.data.size())
    {
        arduboy.cpu.breakpoints_rd[addr] = rd;
        arduboy.cpu.breakpoints_wr[addr] = wr;
    }
}

static bool highlight_func(ImU8 const* data, size_t off, ImU32& color)
{
    bool r = false;
    if(off < 0x100)
    {
        color = IM_COL32(50, 50, 20, 255);
        r = true;
    }
    if(off < 0x20)
    {
        color = IM_COL32(20, 20, 50, 255);
        r = true;
    }
    if(off < arduboy.cpu.data.size() && (
        arduboy.cpu.breakpoints_rd.test(off) ||
        arduboy.cpu.breakpoints_wr.test(off)))
    {
        color = IM_COL32(100, 50, 50, 255);
        r = true;
    }
    return r;
}

static void hover_func(ImU8 const* data, size_t off)
{
    using namespace ImGui;
    auto const* sym = arduboy.symbol_for_data_addr((uint16_t)off);
    if(!sym) return;
    BeginTooltip();
    if(sym->size > 1)
        Text("%s [byte %d]", sym->name.c_str(), int(off - sym->addr));
    else
        TextUnformatted(sym->name.c_str());
    EndTooltip();
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
