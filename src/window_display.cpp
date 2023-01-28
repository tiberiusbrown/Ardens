#include "imgui.h"

#include "absim.hpp"

extern absim::arduboy_t arduboy;

void window_display(bool& open, void* tex)
{
	using namespace ImGui;

    if(open)
    {
        if(Begin("Display", &open,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse))
        {
            AlignTextToFramePadding();
            TextUnformatted("Filter for:");
            int& num = arduboy.display.num_pixel_history;
            SameLine();
            if(RadioButton("Monochrome", num <= 1))
                num = 1;
            SameLine();
            if(RadioButton("3-Level", num == 2))
                num = 2;
            SameLine();
            if(RadioButton("4-Level", num == 3))
                num = 3;
            auto t = GetContentRegionAvail();
            float w = 128, h = 64;
            while(w + 128 < t.x && h + 64 < t.y)
                w += 128, h += 64;
            Image(tex, { w, h });
        }
        End();
    }
}
