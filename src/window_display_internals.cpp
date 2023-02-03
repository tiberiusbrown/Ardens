#include "imgui.h"
#include "imgui_memory_editor.h"

#include "absim.hpp"

extern absim::arduboy_t arduboy;
static MemoryEditor memed_display_ram;

static char const* const MODES[] =
{
    "HORIZONTAL",
    "VERTICAL",
    "PAGE",
    "INVALID"
};

void window_display_internals(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 200, 400 }, ImGuiCond_FirstUseEver);
    if(Begin("Display Internals", &open) && arduboy.cpu.decoded)
    {
        auto const& d = arduboy.display;
        Text("Clock Divider          %d", d.divide_ratio);
        Text("Precharge Phase 1      %d cycles", d.phase_1);
        Text("Precharge Phase 2      %d cycles", d.phase_2);
        Text("Fosc                   %.1f kHz", d.fosc() / 1000.f);
        Text("Frame Rate             %.1f Hz", d.refresh_rate());
        Separator();
        Text("Display                %s", d.display_on ? "ON" : "OFF");
        Text("Inverse Display        %s", d.inverse_display ? "ON" : "OFF");
        Text("Entire Display On      %s", d.entire_display_on ? "ON" : "OFF");
        Text("Charge Pump            %s", d.enable_charge_pump ? "ON" : "OFF");
        Text("Contrast               %d", d.contrast);
        Text("Mux Ratio              %d", d.mux_ratio + 1);
        Text("COM Scan Direction     %s", d.com_scan_direction ? "REVERSE" : "FORWARD");
        Separator();
        Text("Addressing Mode        %s", MODES[std::min(4u, (unsigned)d.addressing_mode)]);
        Text("Column Start           %d", d.col_start);
        Text("Column End             %d", d.col_end);
        Text("Page Start             %d", d.page_start);
        Text("Page End               %d", d.page_end);
        Text("Column Start (Page)    %d", d.page_col_start);
        Text("Page Start (Page)      %d", d.page_page_start);
        Text("Segment Remap          %s", d.segment_remap ? "ON" : "OFF");
        Separator();
        memed_display_ram.DrawContents(
            arduboy.display.ram.data(),
            arduboy.display.ram.size());
    }
    End();
}
