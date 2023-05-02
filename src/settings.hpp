#pragma once

#include <array>

enum
{
    FILTER_NONE,
    FILTER_SCALE2X,
    FILTER_SCALE3X,
    FILTER_SCALE4X,

    FILTER_MIN = FILTER_NONE,
    FILTER_MAX = FILTER_SCALE4X,
};

constexpr int RECORDING_ZOOM_MAX = 4;

struct settings_t
{

    bool open_disassembly = true;
    bool open_display = true;
    bool open_display_buffer = false;
    bool open_display_internals = true;
    bool open_data_space = true;
    bool open_profiler = true;
    bool open_simulation = true;
    bool open_call_stack = true;
    bool open_symbols = true;
    bool open_globals = true;
    bool open_fx_data = false;
    bool open_fx_internals = false;
    bool open_eeprom = false;
    bool open_cpu_usage = false;
    bool open_led = false;
    bool open_serial = false;
    bool open_sound = false;

    bool fullzoom = false;

    bool profiler_cycle_counts = false;
    bool profiler_group_symbols = false;
    bool enable_step_breaks = true;
    bool enable_stack_breaks = true;

    int display_filtering = FILTER_NONE;
    int display_downsample = 1;
    int recording_filtering = FILTER_NONE;
    int recording_downsample = 1;
    int recording_zoom = 1;
    int num_pixel_history = 1;
};

extern settings_t settings;

void init_settings();
void update_settings();
