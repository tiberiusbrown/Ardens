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
    int d_display;
    int d_simulation;
    int d_disassembly;
    int d_symbols;
    int d_globals;
    int d_call_stack;
    int d_display_internals;
    int d_profiler;
    int d_data_space;
    int d_profiler_cycle_counts;
    int d_num_pixel_history;
    if(sscanf(line, "open_display=%d", &d_display) == 1) settings.open_display = !!d_display;
    if(sscanf(line, "open_simulation=%d", &d_simulation) == 1) settings.open_simulation = !!d_simulation;
    if(sscanf(line, "open_disassembly=%d", &d_disassembly) == 1) settings.open_disassembly = !!d_disassembly;
    if(sscanf(line, "open_symbols=%d", &d_symbols) == 1) settings.open_symbols = !!d_symbols;
    if(sscanf(line, "open_globals=%d", &d_globals) == 1) settings.open_globals = !!d_globals;
    if(sscanf(line, "open_call_stack=%d", &d_call_stack) == 1) settings.open_call_stack = !!d_call_stack;
    if(sscanf(line, "open_display_internals=%d", &d_display_internals) == 1) settings.open_display_internals = !!d_display_internals;
    if(sscanf(line, "open_profiler=%d", &d_profiler) == 1) settings.open_profiler = !!d_profiler;
    if(sscanf(line, "open_data_space=%d", &d_data_space) == 1) settings.open_data_space = !!d_data_space;
    if(sscanf(line, "num_pixel_history=%d", &d_num_pixel_history) == 1) settings.num_pixel_history = d_num_pixel_history;
}
static void settings_write_all(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    buf->reserve(buf->size() + 1024);
    buf->append("[UserData][Settings]\n");
    buf->appendf("open_display=%d\n", (int)settings.open_display);
    buf->appendf("open_simulation=%d\n", (int)settings.open_simulation);
    buf->appendf("open_disassembly=%d\n", (int)settings.open_disassembly);
    buf->appendf("open_symbols=%d\n", (int)settings.open_symbols);
    buf->appendf("open_globals=%d\n", (int)settings.open_globals);
    buf->appendf("open_call_stack=%d\n", (int)settings.open_call_stack);
    buf->appendf("open_display_internals=%d\n", (int)settings.open_display_internals);
    buf->appendf("open_profiler=%d\n", (int)settings.open_profiler);
    buf->appendf("open_data_space=%d\n", (int)settings.open_data_space);
    buf->appendf("profiler_cycle_counts=%d\n", (int)settings.profiler_cycle_counts);
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
