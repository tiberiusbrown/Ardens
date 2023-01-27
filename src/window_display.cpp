#include "imgui.h"

void window_display(bool& open, void* tex)
{
	using namespace ImGui;

    if(open)
    {
        if(Begin("Display", &open,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse))
        {
            auto t = GetContentRegionAvail();
            float w = t.x, h = w * 0.5f;
            if(h > t.y)
                h = t.y, w = h * 2;
            Image(tex, { w, h });
        }
        End();
    }
}
