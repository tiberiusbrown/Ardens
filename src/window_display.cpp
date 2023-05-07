#include "imgui.h"

#include "common.hpp"

void window_display(bool& open)
{
	using namespace ImGui;

    if(open)
    {
        SetNextWindowSize({ 400, 200 }, ImGuiCond_FirstUseEver);
        if(Begin("Display", &open,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse))
        {
            auto t = GetContentRegionAvail();

            float z = display_filter_zoom();
            float tw = 128 * z;
            float th = 64 * z;
            float w = tw, h = th;
            bool smaller = false;

            if(t.x >= 1.f && t.y >= 1.f)
            {
                while(w > t.x || h > t.y)
                    w *= 0.5f, h *= 0.5f, smaller = true;

                while(w + tw < t.x && h + th < t.y)
                    w += tw, h += th;
            }

            SDL_SetTextureScaleMode(
                display_texture,
                smaller ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);

            Image(display_texture, { w, h });
#if 0
            AlignTextToFramePadding();
            TextUnformatted("Filter for:");
            int num = settings.num_pixel_history;
            SameLine();
            if(RadioButton("Monochrome", num <= 1))
                num = 1;
            SameLine();
            if(RadioButton("3-Level", num == 2))
                num = 2;
            if(IsItemHovered())
            {
                BeginTooltip();
                TextUnformatted("Average the last 2 display frames");
                EndTooltip();
            }
            SameLine();
            if(RadioButton("4-Level", num == 3))
                num = 3;
            if(IsItemHovered())
            {
                BeginTooltip();
                TextUnformatted("Average the last 3 display frames");
                EndTooltip();
            }
            if(num != settings.num_pixel_history)
            {
                settings.num_pixel_history = num;
                update_settings();
            }
#endif
        }
        End();
    }
}