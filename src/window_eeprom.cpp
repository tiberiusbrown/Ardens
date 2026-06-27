#include "imgui.h"
#include "imgui_memory_editor.h"

#include "common.hpp"

static MemoryEditor memed_eeprom;

void window_eeprom(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 400 * app.pixel_ratio, 400 * app.pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("EEPROM", &open) && app.emulator.core_state.cpu.decoded)
    {
        memed_eeprom.ReadOnly = !app.emulator.is_present_state();
        memed_eeprom.DrawContents(
            app.emulator.core_state.cpu.eeprom.data(),
            app.emulator.core_state.cpu.eeprom.size());
    }
    End();
}
