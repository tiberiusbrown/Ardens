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
    ABSIM_BOOL_SETTING(profiler_cycle_counts);
    ABSIM_BOOL_SETTING(enable_step_breaks);
    ABSIM_BOOL_SETTING(enable_stack_breaks);
    ABSIM_BOOL_SETTING(fullzoom);

#undef ABSIM_BOOL_SETTING

    int d_display_filtering;
    if(sscanf(line, "display_filtering=%d", &d_display_filtering) == 1) settings.display_filtering = d_display_filtering;
    if(settings.display_filtering < FILTER_MIN) settings.display_filtering = FILTER_MIN;
    if(settings.display_filtering > FILTER_MAX) settings.display_filtering = FILTER_MAX;

    int d_recording_filtering;
    if(sscanf(line, "recording_filtering=%d", &d_recording_filtering) == 1) settings.recording_filtering = d_recording_filtering;
    if(settings.recording_filtering < FILTER_MIN) settings.recording_filtering = FILTER_MIN;
    if(settings.recording_filtering > FILTER_MAX) settings.recording_filtering = FILTER_MAX;

    int d_recording_zoom;
    if(sscanf(line, "recording_zoom=%d", &d_recording_zoom) == 1) settings.recording_zoom = d_recording_zoom;
    if(settings.recording_zoom < 1) settings.recording_zoom = 1;
    if(settings.recording_zoom > RECORDING_ZOOM_MAX) settings.recording_zoom = RECORDING_ZOOM_MAX;

    int d_num_pixel_history;
    if(sscanf(line, "num_pixel_history=%d", &d_num_pixel_history) == 1) settings.num_pixel_history = d_num_pixel_history;
    if(settings.num_pixel_history < 1) settings.num_pixel_history = 1;
    if(settings.num_pixel_history > 3) settings.num_pixel_history = 3;
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
    ABSIM_BOOL_SETTING(profiler_cycle_counts);
    ABSIM_BOOL_SETTING(enable_step_breaks);
    ABSIM_BOOL_SETTING(enable_stack_breaks);
    ABSIM_BOOL_SETTING(fullzoom);

#undef ABSIM_BOOL_SETTING

    buf->appendf("display_filtering=%d\n", settings.display_filtering);
    buf->appendf("recording_filtering=%d\n", settings.recording_filtering);
    buf->appendf("recording_zoom=%d\n", settings.recording_zoom);
    buf->appendf("num_pixel_history=%d\n", settings.num_pixel_history);
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
