#pragma once

#include <array>

enum
{
    FILTER_NONE,
    FILTER_SCALE2X,
    FILTER_SCALE3X,
    FILTER_SCALE4X,

    FILTER_HQ2X,
    FILTER_HQ3X,
    FILTER_HQ4X,

    FILTER_MAX_PLUS_ONE,
    FILTER_MIN = FILTER_NONE,
    FILTER_MAX = FILTER_MAX_PLUS_ONE - 1,
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
    bool record_wav = false;
    bool display_auto_filter = true;
    bool profiler_cycle_counts = false;
    bool profiler_group_symbols = false;
    bool enable_step_breaks = true;

    struct
    {
        bool stack_overflow = true;
        bool null_deref = true;
        bool null_rel_deref = true;
        bool oob_deref = true;
        bool oob_eeprom = true;
        bool oob_ijmp = true;
        bool oob_pc = true;
        bool unknown_instr = true;
        bool spi_wcol = true;
        bool fx_busy = true;

        bool& index(int i)
        {
            return reinterpret_cast<bool*>(this)[i - 1];
        }
    } ab;

    int display_palette = 0;
    int display_filtering = FILTER_NONE;
    int display_downsample = 1;
    int display_pixel_grid = 0;
    int recording_palette = 0;
    int recording_filtering = FILTER_NONE;
    int recording_downsample = 1;
    int recording_zoom = 1;
};

extern settings_t settings;

void init_settings();
void update_settings();
