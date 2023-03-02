#include "imgui.h"

#include "absim.hpp"
#include "settings.hpp"

extern absim::arduboy_t arduboy;
extern int fx_data_scroll_addr;

void window_fx_internals(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 300, 100 }, ImGuiCond_FirstUseEver);
    if(Begin("FX Internals", &open) && arduboy.cpu.decoded)
    {
        if(arduboy.fx.command)
        {
            Text("Processing command: %s", arduboy.fx.command);
            Text("Internal address: 0x%06x", arduboy.fx.current_addr);
            SameLine();
            if(SmallButton("Jump"))
            {
                settings.open_fx_data = true;
                update_settings();
                fx_data_scroll_addr = (int)arduboy.fx.current_addr;
            }
        }
        else
        {
            TextDisabled("Processing command: <none>");
            TextDisabled("Internal address: N/A");
        }
        if(arduboy.fx.busy_ps_rem > 0)
            Text("BUSY: %.3f ms", double(arduboy.fx.busy_ps_rem) / 1e9);
        else
            TextDisabled("BUSY: no");
    }
    End();
}
