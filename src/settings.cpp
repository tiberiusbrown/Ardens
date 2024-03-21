#include "settings.hpp"
#include "common.hpp"

#include "imgui.h"
#include "imgui_internal.h"

static ImGuiSettingsHandler settings_handler;

settings_t settings;

static void* settings_read_open(
    ImGuiContext*, ImGuiSettingsHandler*, const char*)
{
    return &settings_handler;
}
static void settings_read_line(
    ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line)
{
#define ARDENS_BOOL_SETTING(n__) \
    { int d; if(sscanf(line, #n__ "=%d", &d) == 1) settings.n__ = !!d; }

#define ARDENS_INT_SETTING(n__, min__, max__) do { int d; \
    if(sscanf(line, #n__ "=%d", &d) == 1) settings.n__ = d; \
    if(settings.n__ < min__) settings.n__ = min__; \
    if(settings.n__ > max__) settings.n__ = max__; \
    } while(0)
    
    ARDENS_BOOL_SETTING(open_display);
    ARDENS_BOOL_SETTING(open_display_buffer);
    ARDENS_BOOL_SETTING(open_display_internals);
    ARDENS_BOOL_SETTING(open_simulation);
    ARDENS_BOOL_SETTING(open_disassembly);
    ARDENS_BOOL_SETTING(open_symbols);
    ARDENS_BOOL_SETTING(open_globals);
    ARDENS_BOOL_SETTING(open_locals);
    ARDENS_BOOL_SETTING(open_call_stack);
    ARDENS_BOOL_SETTING(open_profiler);
    ARDENS_BOOL_SETTING(open_data_space);
    ARDENS_BOOL_SETTING(open_fx_data);
    ARDENS_BOOL_SETTING(open_fx_internals);
    ARDENS_BOOL_SETTING(open_eeprom);
    ARDENS_BOOL_SETTING(open_cpu_usage);
    ARDENS_BOOL_SETTING(open_led);
    ARDENS_BOOL_SETTING(open_serial);
    ARDENS_BOOL_SETTING(open_sound);
    ARDENS_BOOL_SETTING(open_source);
    ARDENS_BOOL_SETTING(open_progmem);

    ARDENS_BOOL_SETTING(profiler_cycle_counts);
    ARDENS_BOOL_SETTING(profiler_group_symbols);
    ARDENS_BOOL_SETTING(enable_step_breaks);
    ARDENS_BOOL_SETTING(fullzoom);
    ARDENS_BOOL_SETTING(display_integer_scale);
    ARDENS_BOOL_SETTING(display_auto_filter);
    ARDENS_BOOL_SETTING(record_wav);
    ARDENS_BOOL_SETTING(recording_sameasdisplay);
    ARDENS_BOOL_SETTING(nondeterminism);
    ARDENS_BOOL_SETTING(frame_based_cpu_usage);

    ARDENS_BOOL_SETTING(ab.stack_overflow);
    ARDENS_BOOL_SETTING(ab.null_deref);
    ARDENS_BOOL_SETTING(ab.oob_deref);
    ARDENS_BOOL_SETTING(ab.oob_eeprom);
    ARDENS_BOOL_SETTING(ab.oob_ijmp);
    ARDENS_BOOL_SETTING(ab.oob_pc);
    ARDENS_BOOL_SETTING(ab.unknown_instr);
    ARDENS_BOOL_SETTING(ab.spi_wcol);
    ARDENS_BOOL_SETTING(ab.fx_busy);

    ARDENS_INT_SETTING(display_current_modeling, 0, 3);
    ARDENS_INT_SETTING(display_palette, PALETTE_MIN, PALETTE_MAX);
    ARDENS_INT_SETTING(display_filtering, FILTER_MIN, FILTER_MAX);
    ARDENS_INT_SETTING(display_downsample, 1, 4);
    ARDENS_INT_SETTING(display_pixel_grid, PGRID_MIN, PGRID_MAX);
    ARDENS_INT_SETTING(display_orientation, 0, 3);
    ARDENS_INT_SETTING(recording_palette, PALETTE_MIN, PALETTE_MAX);
    ARDENS_INT_SETTING(recording_filtering, FILTER_MIN, FILTER_MAX);
    ARDENS_INT_SETTING(recording_downsample, 1, 4);
    ARDENS_INT_SETTING(recording_zoom, 1, 4);
    ARDENS_INT_SETTING(recording_orientation, 0, 3);
    ARDENS_INT_SETTING(uiscale, 0, 6);
    ARDENS_INT_SETTING(volume, 0, 200);

#undef ARDENS_BOOL_SETTING
#undef ARDENS_INT_SETTING
}

static void settings_write_all(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    buf->reserve(buf->size() + 1024);
    buf->append("[UserData][Settings]\n");

#define ARDENS_BOOL_SETTING(n__) \
    buf->appendf(#n__ "=%d\n", (int)settings.n__)

#define ARDENS_INT_SETTING(n__, min__, max__) \
    buf->appendf(#n__ "=%d\n", settings.n__)

    ARDENS_BOOL_SETTING(open_display);
    ARDENS_BOOL_SETTING(open_display_buffer);
    ARDENS_BOOL_SETTING(open_display_internals);
    ARDENS_BOOL_SETTING(open_simulation);
    ARDENS_BOOL_SETTING(open_disassembly);
    ARDENS_BOOL_SETTING(open_symbols);
    ARDENS_BOOL_SETTING(open_globals);
    ARDENS_BOOL_SETTING(open_locals);
    ARDENS_BOOL_SETTING(open_call_stack);
    ARDENS_BOOL_SETTING(open_profiler);
    ARDENS_BOOL_SETTING(open_data_space);
    ARDENS_BOOL_SETTING(open_fx_data);
    ARDENS_BOOL_SETTING(open_fx_internals);
    ARDENS_BOOL_SETTING(open_eeprom);
    ARDENS_BOOL_SETTING(open_cpu_usage);
    ARDENS_BOOL_SETTING(open_led);
    ARDENS_BOOL_SETTING(open_serial);
    ARDENS_BOOL_SETTING(open_sound);
    ARDENS_BOOL_SETTING(open_source);
    ARDENS_BOOL_SETTING(open_progmem);

    ARDENS_BOOL_SETTING(profiler_cycle_counts);
    ARDENS_BOOL_SETTING(profiler_group_symbols);
    ARDENS_BOOL_SETTING(enable_step_breaks);
    ARDENS_BOOL_SETTING(fullzoom);
    ARDENS_BOOL_SETTING(display_integer_scale);
    ARDENS_BOOL_SETTING(display_auto_filter);
    ARDENS_BOOL_SETTING(display_current_modeling);
    ARDENS_BOOL_SETTING(record_wav);
    ARDENS_BOOL_SETTING(recording_sameasdisplay);
    ARDENS_BOOL_SETTING(nondeterminism);
    ARDENS_BOOL_SETTING(frame_based_cpu_usage);

    ARDENS_BOOL_SETTING(ab.stack_overflow);
    ARDENS_BOOL_SETTING(ab.null_deref);
    ARDENS_BOOL_SETTING(ab.oob_deref);
    ARDENS_BOOL_SETTING(ab.oob_eeprom);
    ARDENS_BOOL_SETTING(ab.oob_ijmp);
    ARDENS_BOOL_SETTING(ab.oob_pc);
    ARDENS_BOOL_SETTING(ab.unknown_instr);
    ARDENS_BOOL_SETTING(ab.spi_wcol);
    ARDENS_BOOL_SETTING(ab.fx_busy);

    ARDENS_INT_SETTING(display_palette, PALETTE_MIN, PALETTE_MAX);
    ARDENS_INT_SETTING(display_filtering, FILTER_MIN, FILTER_MAX);
    ARDENS_INT_SETTING(display_downsample, 1, 4);
    ARDENS_INT_SETTING(display_pixel_grid, PGRID_MIN, PGRID_MAX);
    ARDENS_INT_SETTING(display_orientation, 0, 3);
    ARDENS_INT_SETTING(recording_palette, PALETTE_MIN, PALETTE_MAX);
    ARDENS_INT_SETTING(recording_filtering, FILTER_MIN, FILTER_MAX);
    ARDENS_INT_SETTING(recording_downsample, 1, 4);
    ARDENS_INT_SETTING(recording_zoom, 1, 4);
    ARDENS_INT_SETTING(recording_orientation, 0, 3);
    ARDENS_INT_SETTING(uiscale, 0, 6);
    ARDENS_INT_SETTING(volume, 0, 200);

#undef ARDENS_BOOL_SETTING
#undef ARDENS_INT_SETTING

    buf->append("\n");
}

void init_settings()
{
    settings_handler.TypeName = "UserData";
    settings_handler.TypeHash = ImHashStr("UserData");
    settings_handler.ReadOpenFn = settings_read_open;
    settings_handler.ReadLineFn = settings_read_line;
    settings_handler.WriteAllFn = settings_write_all;
    ImGui::AddSettingsHandler(&settings_handler);
}

void update_settings()
{
    if(settings.recording_sameasdisplay)
    {
        settings.recording_palette = settings.display_palette;
        settings.recording_filtering = settings.display_filtering;
        settings.recording_downsample = settings.display_downsample;
        settings.recording_orientation = settings.display_orientation;
    }
    ImGui::MarkIniSettingsDirty();
}

void autoset_from_device_type()
{
    if(arduboy->device_type == "ArduboyFX")
        settings.fxport = FXPORT_D1;
    if(arduboy->device_type == "ArduboyFXDevKit")
        settings.fxport = FXPORT_D2;
    if(arduboy->device_type == "ArduboyMini")
        settings.fxport = FXPORT_E2;
}
