#include "imgui.h"

#include "common.hpp"

static char const* CMD_STRS[absim::w25q128_t::NUM_CMDS] =
{
    "None",
    "Release Power-down",
    "Page Program",
    "Read Data",
    "Write Disable",
    "Read Status Register-1",
    "Write Enable",
    "Sector Erase",
    "<unknown>",
};

void window_fx_internals(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 300 * pixel_ratio, 100 * pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("FX Internals", &open) && arduboy->cpu.decoded)
    {
        if(arduboy->fx.command != absim::w25q128_t::CMD_NONE)
        {
            Text("Processing command: %s", CMD_STRS[arduboy->fx.command]);
            Text("Internal address: 0x%06x", arduboy->fx.current_addr);
            SameLine();
            if(SmallButton("Jump"))
            {
                settings.open_fx_data = true;
                update_settings();
                fx_data_scroll_addr = (int)arduboy->fx.current_addr;
            }
        }
        else
        {
            TextDisabled("Processing command: <none>");
            TextDisabled("Internal address: N/A");
        }
        if(arduboy->fx.busy_ps_rem > 0)
            Text("BUSY: %.3f ms", double(arduboy->fx.busy_ps_rem) / 1e9);
        else
            TextDisabled("BUSY: no");
    }
    End();
}
