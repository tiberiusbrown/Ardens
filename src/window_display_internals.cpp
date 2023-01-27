#include "imgui.h"
#include "imgui_memory_editor.h"

#include "absim.hpp"

extern absim::arduboy_t arduboy;
static MemoryEditor memed_display_ram;

void window_display_internals(bool& open)
{
	using namespace ImGui;
    if(open)
    {
        memed_display_ram.DrawWindow(
            "Display Internals",
            arduboy.display.ram.data(),
            arduboy.display.ram.size());
        open = memed_display_ram.Open;
    }
}
