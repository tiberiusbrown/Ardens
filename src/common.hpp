#pragma once

#ifndef ABSIM_VERSION
#define ABSIM_VERSION "[unknown version]"
#endif

#include <SDL.h>
#include "absim.hpp"
#include "settings.hpp"

constexpr uint32_t AUDIO_FREQ = 16000000 / absim::atmega32u4_t::SOUND_CYCLES;

extern std::unique_ptr<absim::arduboy_t> arduboy;
extern SDL_Window* window;
extern SDL_Renderer* renderer;
extern int display_texture_zoom;
extern SDL_Texture* display_texture;
extern SDL_Texture* display_buffer_texture;

#ifdef __EMSCRIPTEN__
void file_download(
    char const* fname,
    char const* download_fname,
    char const* mime_type);
#endif

extern bool gif_recording;
extern uint32_t gif_ms_rem;
void send_gif_frame(int ds, uint8_t const* pixels);
void screen_recording_toggle(uint8_t const* pixels);

extern float pixel_ratio;
extern bool layout_done;
extern bool settings_loaded;
extern bool fs_ready;

extern int profiler_selected_hotspot;
extern int disassembly_scroll_addr;
extern bool scroll_addr_to_top;
extern int simulation_slowdown;
extern int fx_data_scroll_addr;

extern int display_buffer_addr;
extern int display_buffer_w;
extern int display_buffer_h;

uint32_t color_for_index(size_t index);
uint32_t darker_color_for_index(size_t index);

void view_debugger();
void view_player();
struct ImDrawList;
struct ImVec2;
void display_with_scanlines(ImDrawList* d, ImVec2 const& a, ImVec2 const& b);
void settings_modal();

constexpr size_t FFT_NUM_SAMPLES = 4096;
void process_sound_samples();

enum
{
    PALETTE_DEFAULT,
    PALETTE_GAMEBOY,
    PALETTE_LOW_CONTRAST,

    PALETTE_MIN = PALETTE_DEFAULT,
    PALETTE_MAX = PALETTE_LOW_CONTRAST,
};

enum
{
    PGRID_NONE,
    PGRID_NORMAL,
    PGRID_RED,

    PGRID_MIN = PGRID_NONE,
    PGRID_MAX = PGRID_RED,
};

void palette_rgba(int palette, uint8_t x, uint8_t y[4]);
uint8_t* recording_pixels(bool rgba);
int filter_zoom(int f);
int display_filter_zoom();
int recording_filter_zoom();
void recreate_display_texture();
void scalenx(uint8_t* dst, uint8_t const* src, bool rgba);

void window_disassembly(bool& open);
void window_profiler(bool& open);
void window_display(bool& open);
void window_display_buffer(bool& open);
void window_display_internals(bool& open);
void window_data_space(bool& open);
void window_simulation(bool& open);
void window_call_stack(bool& open);
void window_symbols(bool& open);
#ifdef ABSIM_LLVM
void window_globals(bool& open);
#endif
void window_fx_data(bool& open);
void window_fx_internals(bool& open);
void window_eeprom(bool& open);
void window_cpu_usage(bool& open);
void window_led(bool& open);
void window_serial(bool& open);
void window_sound(bool& open);
