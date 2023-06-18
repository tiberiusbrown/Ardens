#include "imgui.h"
#include "imgui_memory_editor.h"

#include "common.hpp"
#include "dwarf.hpp"

#include <fmt/format.h>

static MemoryEditor memed;

static bool highlight_func(ImU8 const* data, size_t off, ImU32& color)
{
    if(auto const* sym = arduboy->symbol_for_prog_addr((uint16_t)off))
    {
        if(sym->object)
        {
            color = darker_color_for_index(sym->color_index);
            return true;
        }
    }
    return false;
}

static void hover_func(ImU8 const* data, size_t off)
{
    (void)data;
    using namespace ImGui;
    auto const* sym = arduboy->symbol_for_prog_addr((uint16_t)off);
    BeginTooltip();
    if(sym)
        symbol_tooltip((uint16_t)off, *sym, true);
    else
        Text("0x%04x", (unsigned)off);
    EndTooltip();
}

void window_progmem(bool& open)
{
    using namespace ImGui;

    {
        static bool first = true;
        if(first)
        {
            memed.OptShowDataPreview = true;
            memed.PreviewDataType = ImGuiDataType_U8;
            memed.HighlightFn = highlight_func;
            memed.HoverFn = hover_func;
            first = false;
        }
    }

    if(open)
    {
        SetNextWindowSize({ 200, 400 }, ImGuiCond_FirstUseEver);
        if(Begin("PROGMEM Space", &open) && arduboy->cpu.decoded)
        {
            memed.DrawContents(
                arduboy->cpu.prog.data(),
                arduboy->cpu.prog.size());
        }
        End();
    }
}
