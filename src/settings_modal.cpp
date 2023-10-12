#include "common.hpp"

#include "imgui.h"

static_assert(sizeof(settings_t::ab) + 1 == absim::AB_NUM, "update settings_t::ab");

static bool settings_begin_table()
{
    float const A = 300.f * pixel_ratio;
    float const B = 150.f * pixel_ratio;
    float const Y = 260.f * pixel_ratio;

    bool r = ImGui::BeginTable(
        "##table", 2,
        ImGuiTableFlags_NoSavedSettings |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY,
        { A + B, Y });

    if(r)
    {
        ImGui::TableSetupColumn("##colA", ImGuiTableColumnFlags_WidthFixed, A);
        ImGui::TableSetupColumn("##colB", ImGuiTableColumnFlags_WidthFixed, B);
    }

    return r;
}

void modal_settings()
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

    static char const* const PALETTE_ITEMS[] =
    {
            "Default", "Retro", "Low Contrast", "High Contrast",
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

    static char const* const ROTATION_ITEMS[] =
    {
        "Normal", "CW 90", "180", "CCW 90",
    };
    constexpr int NUM_ROTATION_ITEMS = sizeof(ROTATION_ITEMS) / sizeof(ROTATION_ITEMS[0]);

    if(BeginTabBar("##tabs"))
    {
        if(BeginTabItem("Interface"))
        {
            if(settings_begin_table())
            {
                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Volume");
                TableSetColumnIndex(1);
                SetNextItemWidth(-1.f);
                if(SliderInt("##volume", &settings.volume, 0, 200, "%d%%"))
                    update_settings();

                EndTable();
            }
            EndTabItem();
        }
        if(BeginTabItem("Display"))
        {
            if(settings_begin_table())
            {
                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Auto Filter");
                if(IsItemHovered())
                {
                    BeginTooltip();
                    TextUnformatted("Apply temporal filtering on the display (necessary to show grayscale)");
                    EndTooltip();
                }
                TableSetColumnIndex(1);
                if(Checkbox("##displayautofilter", &settings.display_auto_filter))
                    update_settings();

                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Integer Scaling");
                TableSetColumnIndex(1);
                if(Checkbox("##displayintegerscale", &settings.display_integer_scale))
                    update_settings();

                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Palette");
                TableSetColumnIndex(1);
                SetNextItemWidth(-1.f);
                if(Combo("##palettecombo", &settings.display_palette,
                    PALETTE_ITEMS, NUM_PALETTE_ITEMS))
                    update_settings();

#ifndef ARDENS_NO_SCALING
                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Upsample");
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
                TextUnformatted("Downsample");
                TableSetColumnIndex(1);
                SetNextItemWidth(-1.f);
                if(SliderInt("##displaydown", &settings.display_downsample, 1, 4, "%dx"))
                    update_settings();
#endif

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
                TextUnformatted("Orientation");
                TableSetColumnIndex(1);
                SetNextItemWidth(-1.f);
                if(Combo("##porient", &settings.display_orientation,
                    ROTATION_ITEMS, NUM_ROTATION_ITEMS, NUM_ROTATION_ITEMS))
                    update_settings();

                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Pixel Grid");
                TableSetColumnIndex(1);
                SetNextItemWidth(-1.f);
                if(Combo("##pgrid", &settings.display_pixel_grid,
                    PGRID_ITEMS, NUM_PGRID_ITEMS, NUM_PGRID_ITEMS))
                    update_settings();

                EndTable();
            }
            EndTabItem();
        }

        if(BeginTabItem("Recording"))
        {
            if(settings_begin_table())
            {
                if(gif_recording || wav_recording) BeginDisabled();

                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Sync With Display Settings");
                TableSetColumnIndex(1);
                SetNextItemWidth(-1.f);
                if(Checkbox("##recordingpalettecombo", &settings.recording_sameasdisplay))
                    update_settings();

                if(settings.recording_sameasdisplay) BeginDisabled();

                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Palette");
                TableSetColumnIndex(1);
                SetNextItemWidth(-1.f);
                if(Combo("##recordingpalettecombo", &settings.recording_palette,
                    PALETTE_ITEMS, NUM_PALETTE_ITEMS))
                    update_settings();

#ifndef ARDENS_NO_SCALING
                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Upsample");
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
                TextUnformatted("Downsample");
                TableSetColumnIndex(1);
                SetNextItemWidth(-1.f);
                if(SliderInt("##recordingdown", &settings.recording_downsample, 1, 4, "%dx"))
                    update_settings();
#endif

                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Orientation");
                TableSetColumnIndex(1);
                SetNextItemWidth(-1.f);
                if(Combo("##recordingorient", &settings.recording_orientation,
                    ROTATION_ITEMS, NUM_ROTATION_ITEMS, NUM_ROTATION_ITEMS))
                    update_settings();

                if(settings.recording_sameasdisplay) EndDisabled();

                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Zoom");
                TableSetColumnIndex(1);
                SetNextItemWidth(-1.f);
                if(SliderInt("##recordingzoom", &settings.recording_zoom, 1, RECORDING_ZOOM_MAX, "%dx"))
                    update_settings();

                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Record Audio (WAV)");
                TableSetColumnIndex(1);
                if(Checkbox("##recordwav", &settings.record_wav))
                    update_settings();

                if(gif_recording || wav_recording) EndDisabled();

                EndTable();
            }
            EndTabItem();
        }

        if(BeginTabItem("Breakpoints"))
        {
            if(settings_begin_table())
            {
                TableNextRow();
                TableSetColumnIndex(0);
                AlignTextToFramePadding();
                TextUnformatted("Enable breakpoints while stepping over/out");
                TableSetColumnIndex(1);
                if(Checkbox("##stepbreak", &settings.enable_step_breaks))
                    update_settings();

                static std::array<char const*, absim::AB_NUM> const ABSTRS =
                {
                    nullptr,
                    "Stack Overflow",
                    "Null Dereference",
                    "Null-Relative Dereference",
                    "Out-of-bounds Dereference",
                    "EEPROM Out-of-bounds",
                    "Out-of-bounds Indirect Jump",
                    "Out-of-bounds PC",
                    "Unknown Instruction",
                    "SPI Write Collision",
                    "FX Busy",
                };

                static_assert(sizeof(settings_t::ab) == absim::AB_NUM - 1);

                for(int i = 1; i < absim::AB_NUM; ++i)
                {
                    TableNextRow();
                    TableSetColumnIndex(0);
                    AlignTextToFramePadding();
                    Text("Auto-break: %s", ABSTRS[i]);
                    TableSetColumnIndex(1);
                    PushID(i);
                    if(Checkbox("##autobreak", &settings.ab.index(i)))
                        update_settings();
                    PopID();
                }


                EndTable();
            }
            EndTabItem();
        }

        EndTabBar();
    }

    if(Button("OK", ImVec2(120 * pixel_ratio, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter))
        CloseCurrentPopup();

    EndPopup();
}
