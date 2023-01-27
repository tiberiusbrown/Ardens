#include "imgui.h"
#include "imgui_memory_editor.h"

#include "absim.hpp"

extern absim::arduboy_t arduboy;
static MemoryEditor memed_data_space;

void window_data_space(bool& open)
{
    using namespace ImGui;
    if(open)
    {
        memed_data_space.DrawWindow(
            "CPU Data Space",
            arduboy.cpu.data.data(),
            arduboy.cpu.data.size());
        open = memed_data_space.Open;
    }
}
