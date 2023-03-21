#include "common.hpp"

#include "imgui.h"

void settings_modal()
{
    using namespace ImGui;

    if(IsKeyPressed(ImGuiKey_0, false) && !IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
        OpenPopup("Settings");

    if(!BeginPopupModal("Settings", NULL,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings))
        return;

    if(BeginTable("##table", 2,
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_BordersH |
        ImGuiTableFlags_ScrollY,
        ImVec2(0.f, GetFontSize() * 20.f)))
    {
        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Display Levels");
        TableSetColumnIndex(1);
        {
            int num = settings.num_pixel_history;
            if(RadioButton("Monochrome", num <= 1))
                num = 1;
            //SameLine();
            if(RadioButton("3-Level", num == 2))
                num = 2;
            if(IsItemHovered())
            {
                BeginTooltip();
                TextUnformatted("Average the last 2 display frames");
                EndTooltip();
            }
            //SameLine();
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
        }

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Auto-break on stack overflow");
        TableSetColumnIndex(1);
        if(Checkbox("##autobreak", &settings.enable_stack_breaks))
            update_settings();

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Enable breakpoints while stepping over/out");
        TableSetColumnIndex(1);
        if(Checkbox("##stepbreak", &settings.enable_step_breaks))
            update_settings();

        EndTable();
    }

    if(Button("OK", ImVec2(120, 0)))
        CloseCurrentPopup();

    EndPopup();
}
