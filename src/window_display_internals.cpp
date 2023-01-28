#include "imgui.h"
#include "imgui_memory_editor.h"

#include "absim.hpp"

extern absim::arduboy_t arduboy;
static MemoryEditor memed_display_ram;

void window_display_internals(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 200, 400 }, ImGuiCond_FirstUseEver);
    if(Begin("Display Internals", &open) && arduboy.cpu.decoded)
    {
        memed_display_ram.DrawContents(
            arduboy.display.ram.data(),
            arduboy.display.ram.size());
    }
    End();
}
