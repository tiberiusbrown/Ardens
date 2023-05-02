#include "settings.hpp"

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
#define ABSIM_BOOL_SETTING(n__) \
    { int d; if(sscanf(line, #n__ "=%d", &d) == 1) settings.n__ = !!d; }
    
    ABSIM_BOOL_SETTING(open_display);
    ABSIM_BOOL_SETTING(open_display_buffer);
    ABSIM_BOOL_SETTING(open_display_internals);
    ABSIM_BOOL_SETTING(open_simulation);
    ABSIM_BOOL_SETTING(open_disassembly);
    ABSIM_BOOL_SETTING(open_symbols);
    ABSIM_BOOL_SETTING(open_globals);
    ABSIM_BOOL_SETTING(open_call_stack);
    ABSIM_BOOL_SETTING(open_profiler);
    ABSIM_BOOL_SETTING(open_data_space);
    ABSIM_BOOL_SETTING(open_fx_data);
    ABSIM_BOOL_SETTING(open_fx_internals);
    ABSIM_BOOL_SETTING(open_eeprom);
    ABSIM_BOOL_SETTING(open_cpu_usage);
    ABSIM_BOOL_SETTING(open_led);
    ABSIM_BOOL_SETTING(open_serial);
    ABSIM_BOOL_SETTING(open_sound);
    ABSIM_BOOL_SETTING(profiler_cycle_counts);
    ABSIM_BOOL_SETTING(profiler_group_symbols);
    ABSIM_BOOL_SETTING(enable_step_breaks);
    ABSIM_BOOL_SETTING(enable_stack_breaks);
    ABSIM_BOOL_SETTING(fullzoom);

#undef ABSIM_BOOL_SETTING

#define ABSIM_INT_SETTING(n__, min__, max__) do { int d; \
    if(sscanf(line, #n__ "=%d", &d) == 1) settings.n__ = d; \
    if(settings.n__ < min__) settings.n__ = min__; \
    if(settings.n__ > max__) settings.n__ = max__; \
    } while(0)

    ABSIM_INT_SETTING(display_filtering, FILTER_MIN, FILTER_MAX);
    ABSIM_INT_SETTING(display_downsample, 1, 4);
    ABSIM_INT_SETTING(recording_filtering, FILTER_MIN, FILTER_MAX);
    ABSIM_INT_SETTING(recording_downsample, 1, 4);
    ABSIM_INT_SETTING(recording_zoom, 1, 4);
    ABSIM_INT_SETTING(num_pixel_history, 1, 3);

#undef ABSIM_INT_SETTING
}

static void settings_write_all(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    buf->reserve(buf->size() + 1024);
    buf->append("[UserData][Settings]\n");

#define ABSIM_BOOL_SETTING(n__) \
    buf->appendf(#n__ "=%d\n", (int)settings.n__)

    ABSIM_BOOL_SETTING(open_display);
    ABSIM_BOOL_SETTING(open_display_buffer);
    ABSIM_BOOL_SETTING(open_display_internals);
    ABSIM_BOOL_SETTING(open_simulation);
    ABSIM_BOOL_SETTING(open_disassembly);
    ABSIM_BOOL_SETTING(open_symbols);
    ABSIM_BOOL_SETTING(open_globals);
    ABSIM_BOOL_SETTING(open_call_stack);
    ABSIM_BOOL_SETTING(open_profiler);
    ABSIM_BOOL_SETTING(open_data_space);
    ABSIM_BOOL_SETTING(open_fx_data);
    ABSIM_BOOL_SETTING(open_fx_internals);
    ABSIM_BOOL_SETTING(open_eeprom);
    ABSIM_BOOL_SETTING(open_cpu_usage);
    ABSIM_BOOL_SETTING(open_led);
    ABSIM_BOOL_SETTING(open_serial);
    ABSIM_BOOL_SETTING(open_sound);
    ABSIM_BOOL_SETTING(profiler_cycle_counts);
    ABSIM_BOOL_SETTING(profiler_group_symbols);
    ABSIM_BOOL_SETTING(enable_step_breaks);
    ABSIM_BOOL_SETTING(enable_stack_breaks);
    ABSIM_BOOL_SETTING(fullzoom);

#undef ABSIM_BOOL_SETTING

#define ABSIM_INT_SETTING(n__) \
    buf->appendf(#n__ "=%d\n", settings.n__)

    ABSIM_INT_SETTING(display_filtering);
    ABSIM_INT_SETTING(display_downsample);
    ABSIM_INT_SETTING(recording_filtering);
    ABSIM_INT_SETTING(recording_downsample);
    ABSIM_INT_SETTING(recording_zoom);
    ABSIM_INT_SETTING(num_pixel_history);

#undef ABSIM_INT_SETTING

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
    ImGui::MarkIniSettingsDirty();
}
