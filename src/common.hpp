#pragma once

#ifndef ABSIM_VERSION
#define ABSIM_VERSION "[unknown version]"
#endif

#include <SDL.h>
#include "absim.hpp"
#include "settings.hpp"

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

void view_debugger();
void view_player();
void settings_modal();

uint8_t* recording_pixels(bool rgba);
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
