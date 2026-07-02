#include "imgui.h"
#include "common.hpp"

#include <array>

void window_linked_secondary_arduboy(bool& open)
{
    using namespace ImGui;

    if(!open)
    {
        app.linked_secondary_input_focus = false;
        return;
    }

    SetNextWindowSize({ 400 * app.pixel_ratio, 240 * app.pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Linked Secondary Arduboy", &open,
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse))
    {
        bool connected = linked_secondary_arduboy_connected();
        app.linked_secondary_input_focus =
            connected && IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        bool can_connect =
            app.emulator.core_state.cpu.decoded &&
            !app.emulator.program_state.prog_filedata.empty();

        if(connected)
        {
            if(Button("Disconnect"))
            {
                disconnect_linked_secondary_arduboy();
                app.linked_secondary_input_focus = false;
            }
            Separator();

            recreate_display_texture(
                app.linked_secondary_display_texture,
                app.linked_secondary_display_texture_zoom);

            if(app.linked_secondary_arduboy)
            {
                update_display_texture(
                    app.linked_secondary_display_texture,
                    app.linked_secondary_arduboy->peripherals.display.filtered_pixels.data());
            }
            else
            {
                static std::array<uint8_t, 128 * 64> blank_pixels{};
                update_display_texture(
                    app.linked_secondary_display_texture,
                    blank_pixels.data());
            }

            draw_display_texture(
                app.linked_secondary_display_texture,
                app.linked_secondary_display_texture_zoom);
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
    }
    else
    {
        app.linked_secondary_input_focus = false;
    }
    End();
}
