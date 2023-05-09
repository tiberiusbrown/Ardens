#include "common.hpp"

#include "imgui.h"

void settings_modal()
{
    using namespace ImGui;

    if(!GetIO().WantCaptureKeyboard &&
        IsKeyPressed(ImGuiKey_O, false) &&
        !IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
        OpenPopup("Settings");

    if(!BeginPopupModal("Settings", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings))
        return;

    if(BeginTable("##table", 2,
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_ScrollY,
        ImVec2(0.f, GetFontSize() * 25.f)))
    {
        float const W =
            CalcTextSize("Low Contrast").x +
            GetFrameHeight() +
            GetStyle().FramePadding.x * 2;

        static char const* const PALETTE_ITEMS[] =
        {
                "Default", "Gameboy", "Low Contrast"
        };
        constexpr int NUM_PALETTE_ITEMS = sizeof(PALETTE_ITEMS) / sizeof(PALETTE_ITEMS[0]);
        static_assert(NUM_PALETTE_ITEMS == PALETTE_MAX + 1, "");

        static char const* const FILTER_ITEMS[] =
        {
                "None",
                "Scale2x", "Scale3x", "Scale4x",
                "HQ2x", "HQ3x", "HQ4x",
        };
        constexpr int NUM_FILTER_ITEMS = sizeof(FILTER_ITEMS) / sizeof(FILTER_ITEMS[0]);
        static_assert(NUM_FILTER_ITEMS == FILTER_MAX + 1, "");

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Display Palette");
        TableSetColumnIndex(1);
        SetNextItemWidth(W);
        if(Combo("##palettecombo", &settings.display_palette,
            PALETTE_ITEMS, NUM_PALETTE_ITEMS))
            update_settings();

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Display Upsample");
        TableSetColumnIndex(1);
        SetNextItemWidth(W);
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
        SetNextItemWidth(W);
        if(SliderInt("##displaydown", &settings.display_downsample, 1, 4, "%dx"))
            update_settings();

        static char const* const PGRID_ITEMS[] =
        {
                "Off", "Normal",
                "Red", "Green", "Blue",
                "Cyan", "Magenta", "Yellow",
                "White",
        };
        constexpr int NUM_PGRID_ITEMS = sizeof(PGRID_ITEMS) / sizeof(PGRID_ITEMS[0]);
        static_assert(NUM_PGRID_ITEMS == PGRID_MAX + 1, "");

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Display Pixel Grid");
        TableSetColumnIndex(1);
        SetNextItemWidth(W);
        if(Combo("##pgrid", &settings.display_pixel_grid,
            PGRID_ITEMS, NUM_PGRID_ITEMS, NUM_PGRID_ITEMS))
            update_settings();

        if(gif_recording) BeginDisabled();

        TableNextRow(0, pixel_ratio * 12);

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Recording Palette");
        TableSetColumnIndex(1);
        SetNextItemWidth(W);
        if(Combo("##recordingpalettecombo", &settings.recording_palette,
            PALETTE_ITEMS, NUM_PALETTE_ITEMS))
            update_settings();

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Recording Upsample");
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
        SetNextItemWidth(W);
        if(SliderInt("##recordingdown", &settings.recording_downsample, 1, 4, "%dx"))
            update_settings();

        TableNextRow();
        TableSetColumnIndex(0);
        AlignTextToFramePadding();
        TextUnformatted("Recording Zoom");
        TableSetColumnIndex(1);
        SetNextItemWidth(W);
        if(SliderInt("##recordingzoom", &settings.recording_zoom, 1, RECORDING_ZOOM_MAX, "%dx"))
            update_settings();

        TableNextRow(0, pixel_ratio * 12);

        if(gif_recording) EndDisabled();

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
