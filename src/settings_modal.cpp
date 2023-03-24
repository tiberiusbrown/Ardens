#include "common.hpp"

#include "imgui.h"

void settings_modal()
{
    using namespace ImGui;

    if(!GetIO().WantCaptureKeyboard &&
        IsKeyPressed(ImGuiKey_O, false) &&
        !IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
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
            if(RadioButton("4-Level", num >= 3))
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

        static char const* const FILTER_ITEMS[] =
        {
                "None", "Scale2x", "Scale3x", "Scale4x",
        };
        constexpr int NUM_FILTER_ITEMS = sizeof(FILTER_ITEMS) / sizeof(FILTER_ITEMS[0]);

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Display Filter");
        TableSetColumnIndex(1);
        SetNextItemWidth(-1.f);
        if(Combo("##filtercombo", &settings.display_filtering,
            FILTER_ITEMS, NUM_FILTER_ITEMS))
            update_settings();

        TableNextRow();
        if(filter_zoom(settings.display_filtering) % settings.display_downsample != 0)
            TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 0, 0, 75));
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Display Downsample");
        TableSetColumnIndex(1);
        SetNextItemWidth(-1.f);
        if(SliderInt("##displaydown", &settings.display_downsample, 1, 4, "%dx"))
            update_settings();

        if(gif_recording) BeginDisabled();

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Recording Filter");
        TableSetColumnIndex(1);
        SetNextItemWidth(-1.f);
        if(Combo("##recordingfiltercombo", &settings.recording_filtering,
            FILTER_ITEMS, NUM_FILTER_ITEMS))
            update_settings();

        TableNextRow();
        if(filter_zoom(settings.recording_filtering) % settings.recording_downsample != 0)
            TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 0, 0, 75));
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Recording Downsample");
        TableSetColumnIndex(1);
        SetNextItemWidth(-1.f);
        if(SliderInt("##recordingdown", &settings.recording_downsample, 1, 4, "%dx"))
            update_settings();

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Recording Zoom");
        TableSetColumnIndex(1);
        SetNextItemWidth(-1.f);
        if(SliderInt("##recordingzoom", &settings.recording_zoom, 1, RECORDING_ZOOM_MAX, "%dx"))
            update_settings();

        if(gif_recording) EndDisabled();

        //{
        //    int f = settings.display_filtering;
        //    if(RadioButton("None", f <= FILTER_NONE))
        //        f = FILTER_NONE;
        //    if(RadioButton("Scale2x", f == FILTER_SCALE2X))
        //        f = FILTER_SCALE2X;
        //    if(RadioButton("Scale3x", f == FILTER_SCALE3X))
        //        f = FILTER_SCALE3X;
        //    if(RadioButton("Scale4x", f >= FILTER_SCALE4X))
        //        f = FILTER_SCALE4X;
        //    if(f != settings.display_filtering)
        //    {
        //        settings.display_filtering = f;
        //        update_settings();
        //    }
        //}

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
