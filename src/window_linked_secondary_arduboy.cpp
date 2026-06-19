#include "imgui.h"
#include "common.hpp"

#include <array>

void window_linked_secondary_arduboy(bool& open)
{
    using namespace ImGui;

    if(!open)
    {
        linked_secondary_input_focus = false;
        return;
    }

    SetNextWindowSize({ 400 * pixel_ratio, 240 * pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Linked Secondary Arduboy", &open,
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse))
    {
        bool connected = linked_secondary_arduboy_connected();
        linked_secondary_input_focus =
            connected && IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        bool can_connect = arduboy.cpu.decoded && !arduboy.prog_filedata.empty();

        if(connected)
        {
            if(Button("Disconnect"))
            {
                disconnect_linked_secondary_arduboy();
                linked_secondary_input_focus = false;
            }
        }
        else
        {
            if(!can_connect)
                BeginDisabled();
            if(Button("Connect Second Arduboy"))
                connect_linked_secondary_arduboy();
            if(!can_connect)
                EndDisabled();
        }

        Separator();

        recreate_display_texture(
            linked_secondary_display_texture,
            linked_secondary_display_texture_zoom);

        if(linked_secondary_arduboy)
        {
            update_display_texture(
                linked_secondary_display_texture,
                linked_secondary_arduboy->display.filtered_pixels.data());
        }
        else
        {
            static std::array<uint8_t, 128 * 64> blank_pixels{};
            update_display_texture(
                linked_secondary_display_texture,
                blank_pixels.data());
        }

        draw_display_texture(
            linked_secondary_display_texture,
            linked_secondary_display_texture_zoom);
    }
    else
    {
        linked_secondary_input_focus = false;
    }
    End();
}
