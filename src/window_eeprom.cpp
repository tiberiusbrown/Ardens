#include "imgui.h"
#include "imgui_memory_editor.h"

#include "common.hpp"

static MemoryEditor memed_eeprom;

void window_eeprom(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 400, 400 }, ImGuiCond_FirstUseEver);
    if(Begin("EEPROM", &open) && arduboy->cpu.decoded)
    {
        memed_eeprom.DrawContents(
            arduboy->cpu.eeprom.data(),
            arduboy->cpu.eeprom.size());
    }
    End();
}
