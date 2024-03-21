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

enum
{
    FXPORT_D1,
    FXPORT_D2,
    FXPORT_E2,
    FXPORT_NUM,
};

enum
{
    DISPLAY_SSD1306,
    DISPLAY_SSD1309,
    DISPLAY_SH1106,
    DISPLAY_NUM,
};

constexpr int RECORDING_ZOOM_MAX = 4;

struct settings_t
{
    // Non-persistent settings
    int fxport = FXPORT_D1;
    int display = DISPLAY_SSD1306;

    bool open_disassembly = false;
    bool open_display = true;
    bool open_display_buffer = false;
    bool open_display_internals = false;
    bool open_data_space = false;
    bool open_profiler = false;
    bool open_simulation = false;
    bool open_call_stack = false;
    bool open_symbols = false;
    bool open_globals = false;
    bool open_locals = false;
    bool open_fx_data = false;
    bool open_fx_internals = false;
    bool open_eeprom = false;
    bool open_cpu_usage = false;
    bool open_led = false;
    bool open_serial = false;
    bool open_sound = false;
    bool open_source = false;
    bool open_progmem = false;
    bool open_timers = false;

    bool fullzoom = false;
    bool record_wav = false;
#if defined(ARDENS_PLAYER) && defined(__EMSCRIPTEN__)
    bool display_integer_scale = false;
#else
    bool display_integer_scale = true;
#endif
    bool display_auto_filter = true;
    bool profiler_cycle_counts = false;
    bool profiler_group_symbols = false;
    bool enable_step_breaks = true;
    bool nondeterminism = false;
    bool frame_based_cpu_usage = true;

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

    int display_current_modeling = 0;
    int display_palette = 0;
    int display_filtering = FILTER_NONE;
    int display_downsample = 1;
    int display_pixel_grid = 0;
    int display_orientation = 0;
    int recording_palette = 0;
    int recording_filtering = FILTER_NONE;
    int recording_downsample = 1;
    int recording_zoom = 1;
    int recording_orientation = 0;

    // 50%, 75%, 100%, 125%, 150%, 175%, 200%
    int uiscale = 2;

    int volume = 100;

    bool recording_sameasdisplay = true;
};

extern settings_t settings;

void init_settings();
void update_settings();
void autoset_from_device_type();
