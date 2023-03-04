#pragma once

#include <array>

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

    bool profiler_cycle_counts = false;
    bool enable_step_breaks = true;
    bool enable_stack_breaks = true;
    bool frame_sync_monochrome = true;

    int num_pixel_history = 1;
};

extern settings_t settings;

void init_settings();
void update_settings();
