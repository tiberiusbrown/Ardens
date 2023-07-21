#include "common.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

void view_debugger()
{
    const bool enableDocking = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable;
    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    if(fs_ready && enableDocking)
    {
#ifdef __EMSCRIPTEN__
        if(!settings_loaded)
        {
            ImGui::LoadIniSettingsFromDisk("/offline/imgui.ini");
            settings_loaded = true;
        }
        if(ImGui::GetIO().WantSaveIniSettings)
        {
            ImGui::GetIO().WantSaveIniSettings = false;
            ImGui::SaveIniSettingsToDisk("/offline/imgui.ini");
            EM_ASM(
                FS.syncfs(function(err) {});
            );
        }
#endif

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGuiWindowFlags host_window_flags = 0;
        host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
        host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        if(dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            host_window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Window", nullptr, host_window_flags);
        ImGui::PopStyleVar(3);

        bool dockspace_created = ImGui::DockBuilderGetNode(dockspace_id) != nullptr;
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, nullptr);
        ImGui::End();

        if(!dockspace_created && !layout_done)
        {
            // default docked layout is just display
            ImGui::DockBuilderDockWindow("Display", dockspace_id);
        }
        layout_done = true;
    }

    if(layout_done)
    {
        bool do_settings_modal = false;
        bool do_about_modal = false;

        if(ImGui::BeginMainMenuBar())
        {
            if(ImGui::BeginMenu("Windows"))
            {
                ImGui::MenuItem("Simulation", nullptr, &settings.open_simulation);
                ImGui::MenuItem("Profiler", nullptr, &settings.open_profiler);
                ImGui::MenuItem("CPU Usage", nullptr, &settings.open_cpu_usage);
                ImGui::MenuItem("Serial Monitor", nullptr, &settings.open_serial);
                ImGui::Separator();
                if(ImGui::BeginMenu("Display"))
                {
                    ImGui::MenuItem("Display", nullptr, &settings.open_display);
                    ImGui::MenuItem("Display Buffer (RAM)", nullptr, &settings.open_display_buffer);
                    ImGui::MenuItem("Display Internals", nullptr, &settings.open_display_internals);
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Flash Chip"))
                {
                    ImGui::MenuItem("FX Data", nullptr, &settings.open_fx_data);
                    ImGui::MenuItem("FX Internals", nullptr, &settings.open_fx_internals);
                    ImGui::EndMenu();
                }
                ImGui::MenuItem("EEPROM", nullptr, &settings.open_eeprom);
                ImGui::MenuItem("LEDs", nullptr, &settings.open_led);
                ImGui::MenuItem("Sound", nullptr, &settings.open_sound);
                ImGui::Separator();
                if(ImGui::BeginMenu("Debugger"))
                {
                    ImGui::MenuItem("Source", nullptr, &settings.open_source);
                    ImGui::MenuItem("Disassembly", nullptr, &settings.open_disassembly);
                    ImGui::MenuItem("Symbols", nullptr, &settings.open_symbols);
                    ImGui::MenuItem("Call Stack", nullptr, &settings.open_call_stack);
                    ImGui::MenuItem("CPU Data Space", nullptr, &settings.open_data_space);
                    ImGui::MenuItem("CPU PROGMEM Space", nullptr, &settings.open_progmem);
#ifdef ARDENS_LLVM
                    ImGui::MenuItem("Globals", nullptr, &settings.open_globals);
                    ImGui::MenuItem("Locals", nullptr, &settings.open_locals);
#endif
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("Tools"))
            {
                if(ImGui::MenuItem("Settings", "O"))
                    do_settings_modal = true;
                if(ImGui::MenuItem("About"))
                    do_about_modal = true;
                if(ImGui::MenuItem("Toggle Player View", "P"))
                {
                    settings.fullzoom = !settings.fullzoom;
                    update_settings();
                }
                if(!arduboy->cpu.decoded) ImGui::BeginDisabled();
                if(ImGui::MenuItem("Take PNG Screenshot", "F2"))
                    save_screenshot();
                if(ImGui::MenuItem("Toggle GIF Recording", "F3"))
                    toggle_recording();
#ifndef ARDENS_NO_SNAPSHOTS
                if(ImGui::MenuItem("Take Snapshot", "F4"))
                    take_snapshot();
#endif
                if(!arduboy->cpu.decoded) ImGui::EndDisabled();
                ImGui::EndMenu();
            }

            if(arduboy->paused)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 255, 255));
                float w = ImGui::CalcTextSize("PAUSED").x;
                if(ImGui::Selectable("PAUSED##paused", false, 0, { w, 0.f }))
                    arduboy->paused = false;
                ImGui::PopStyleColor();
            }

            if(gif_recording)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
                float w = ImGui::CalcTextSize("RECORDING").x;
                if(ImGui::Selectable("RECORDING##recording", false, 0, { w, 0.f }))
                    screen_recording_toggle(nullptr);
                ImGui::PopStyleColor();
            }

            {
                float w = ImGui::CalcTextSize(ARDENS_VERSION, NULL, true).x;
                w += ImGui::GetStyle().ItemSpacing.x;
                ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - w);
                ImGui::MenuItem(ARDENS_VERSION "##version", nullptr, nullptr, false);
            }

            ImGui::EndMainMenuBar();
        }

        if(do_settings_modal)
            ImGui::OpenPopup("Settings");
        if(do_about_modal)
            ImGui::OpenPopup("About");

        if(arduboy->cpu.decoded)
        {
            window_display(settings.open_display);
            window_display_buffer(settings.open_display_buffer);
            window_simulation(settings.open_simulation);
            window_source(settings.open_source);
            window_disassembly(settings.open_disassembly);
            window_symbols(settings.open_symbols);
#ifdef ARDENS_LLVM
            window_globals(settings.open_globals);
            window_locals(settings.open_locals);
#endif
            window_call_stack(settings.open_call_stack);
            window_display_internals(settings.open_display_internals);
            window_profiler(settings.open_profiler);
            window_data_space(settings.open_data_space);
            window_progmem(settings.open_progmem);
            window_fx_data(settings.open_fx_data);
            window_fx_internals(settings.open_fx_internals);
            window_eeprom(settings.open_eeprom);
            window_cpu_usage(settings.open_cpu_usage);
            window_led(settings.open_led);
            window_serial(settings.open_serial);
            window_sound(settings.open_sound);
        }
    }
}
