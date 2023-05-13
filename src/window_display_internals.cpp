#include "imgui.h"
#include "imgui_memory_editor.h"

#include "common.hpp"

static MemoryEditor memed_display_ram;

static char const* const MODES[] =
{
    "HORIZONTAL",
    "VERTICAL",
    "PAGE",
    "INVALID"
};

static bool highlight_func(ImU8 const* data, size_t off, ImU32& color)
{
    bool r = false;

    if(off == arduboy->display.data_page * 128 + arduboy->display.data_col)
    {
        color = IM_COL32(40, 160, 40, 255);
        r = true;
    }
    return r;
}

void window_display_internals(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 200, 400 }, ImGuiCond_FirstUseEver);
    if(Begin("Display Internals", &open) && arduboy->cpu.decoded)
    {
        auto const& d = arduboy->display;
        if(CollapsingHeader("Internal State"))
        {
            Text("Address                0x%03x", d.data_page * 128 + d.data_col);
            Text("    Page               %d", d.data_page);
            Text("    Column             %d", d.data_col);
        }
        if(CollapsingHeader("Clock"))
        {
            Text("Fosc setting           %d", (int)d.fosc_index);
            Text("Fosc                   %.1f kHz", d.fosc() / 1000.f);
            Text("Clock Divider          %d", d.divide_ratio + 1);
            Text("Precharge Phase 1      %d cycle%s", d.phase_1, d.phase_1 > 1 ? "s" : "");
            Text("Precharge Phase 2      %d cycle%s", d.phase_2, d.phase_2 > 1 ? "s" : "");
            Text("Frame Rate             %.1f Hz", d.refresh_rate());
        }
        if(CollapsingHeader("Driver Config"))
        {
            Text("Display                %s", d.display_on ? "ON" : "OFF");
            Text("Inverse Display        %s", d.inverse_display ? "ON" : "OFF");
            Text("Entire Display On      %s", d.entire_display_on ? "ON" : "OFF");
            Text("Charge Pump            %s", d.enable_charge_pump ? "ON" : "OFF");
            Text("Contrast               %d", d.contrast);
            Text("Mux Ratio              %d", d.mux_ratio + 1);
            Text("COM Scan Direction     %s", d.com_scan_direction ? "REVERSE" : "FORWARD");
        }
        if(CollapsingHeader("Addressing Config"))
        {
            Text("Addressing Mode        %s", MODES[std::min(4u, (unsigned)d.addressing_mode)]);
            Text("Column Start           %d", d.col_start);
            Text("Column End             %d", d.col_end);
            Text("Page Start             %d", d.page_start);
            Text("Page End               %d", d.page_end);
            Text("Segment Remap          %s", d.segment_remap ? "ON" : "OFF");
        }
        Separator();
        memed_display_ram.HighlightFn = highlight_func;
        memed_display_ram.DrawContents(
            arduboy->display.ram.data(),
            arduboy->display.ram.size());
    }
    End();
}
