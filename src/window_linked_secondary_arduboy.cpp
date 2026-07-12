#include "imgui.h"
#include "common.hpp"

void window_linked_secondary_arduboy(bool& open)
{
    using namespace ImGui;

    if(!open)
    {
        if(linked_secondary_arduboy_connected())
            disconnect_linked_secondary_arduboy();
        app.linked_secondary_input_focus = false;
        return;
    }

    if(!linked_secondary_arduboy_connected())
        connect_linked_secondary_arduboy();

    SetNextWindowSize({ 400 * app.pixel_ratio, 240 * app.pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Linked Secondary Arduboy", &open,
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse))
    {
        bool connected = linked_secondary_arduboy_connected();
        app.linked_secondary_input_focus =
            connected && IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        if(connected)
        {
            if(Button("Swap with Primary"))
                swap_linked_secondary_arduboy();
            Separator();

            recreate_display_texture(
                app.linked_secondary_display_texture,
                app.linked_secondary_display_texture_zoom);

            update_display_texture(
                app.linked_secondary_display_texture,
                app.linked_secondary_arduboy->peripherals.display.filtered_pixels.data());

            draw_display_texture(
                app.linked_secondary_display_texture,
                app.linked_secondary_display_texture_zoom);
        }
    }
    else
    {
        app.linked_secondary_input_focus = false;
    }
    End();

    if(!open)
        disconnect_linked_secondary_arduboy();
}
