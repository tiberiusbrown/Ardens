#pragma once

#include <string>
#include <stdint.h>
#include <unordered_map>

#include "absim.hpp"
#include "settings.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#define ARDENS_ARCH "x64"
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define ARDENS_ARCH "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARDENS_ARCH "arm64"
#elif defined(__arm__) || defined(_M_ARM)
#define ARDENS_ARCH "arm"
#elif defined(__EMSCRIPTEN__) && (defined(__wasm) || defined(__wasm32) || defined(__wasm32__) || defined(__wasm__))
#define ARDENS_ARCH "wasm"
#elif defined(__EMSCRIPTEN__)
#define ARDENS_ARCH "js"
#else
#define ARDENS_ARCH "unknown"
#endif

#if defined(_WIN32)
#define ARDENS_OS "windows"
#elif defined(__linux__)
#define ARDENS_OS "linux"
#define ARDENS_OS_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
#define ARDENS_OS "macos"
#define ARDENS_OS_MACOS
#elif defined(__EMSCRIPTEN__)
#define ARDENS_OS "web"
#define ARDENS_OS_WEB
#else
#define ARDENS_OS "unknown"
#endif

#ifdef ARDENS_DISTNAME
#define ARDENS_TITLE ARDENS_DISTNAME
#elif defined(ARDENS_PLAYER)
#define ARDENS_TITLE "Ardens Player"
#else
#define ARDENS_TITLE "Ardens"
#endif

constexpr uint32_t AUDIO_FREQ = 16000000 / absim::atmega32u4_t::SOUND_CYCLES;

using texture_t = void*;

extern std::unique_ptr<absim::arduboy_t> arduboy;
extern int display_texture_zoom;
extern texture_t display_texture;
extern texture_t display_buffer_texture;
extern std::string dropfile_err;
extern bool loading_indicator;
extern uint64_t ms_since_start;

constexpr uint64_t MS_SHOW_TOUCH_CONTROLS = 10000;
extern bool first_touch;
extern uint64_t ms_since_touch;
struct touch_point_t { float x, y; };
extern std::unordered_map<uintptr_t, touch_point_t> touch_points;
enum { TOUCH_U, TOUCH_D, TOUCH_L, TOUCH_R, TOUCH_A, TOUCH_B };
struct touched_buttons_t { bool btns[6]; };
struct touch_rect_t { float x0, y0, x1, y1; };
touched_buttons_t touched_buttons(); // defined in view_player
touch_rect_t touch_rect(int btn);

// platform-agnostic functionality (common.cpp)
extern "C" int setparam(char const* name, char const* value);
extern "C" int load_file(char const* filename, uint8_t const* data, size_t size);
void init();
void shutdown();
void frame_logic();
void imgui_content();
bool update_pixel_ratio();
void define_font();
void rebuild_fonts();
void rescale_style();
void save_screenshot();
void toggle_recording();
void take_snapshot();
std::string preferred_title();

float volume_gain();
bool ends_with(std::string const& str, std::string const& end);

// platform-specific functionality
void platform_destroy_texture(texture_t t);
texture_t platform_create_texture(int w, int h);
void platform_update_texture(texture_t t, void const* data, size_t n);
void platform_texture_scale_linear(texture_t t);
void platform_texture_scale_nearest(texture_t t);
void platform_set_clipboard_text(char const* str);
void platform_send_sound();
uint64_t platform_get_ms_dt();
float platform_pixel_ratio();
void platform_destroy_fonts_texture();
void platform_create_fonts_texture();
void platform_open_url(char const* url);
void platform_toggle_fullscreen();
void platform_quit();
void platform_set_title(char const* title);

#ifdef __EMSCRIPTEN__
void file_download(
    char const* fname,
    char const* download_fname,
    char const* mime_type);
#endif

extern bool gif_recording;
extern uint64_t gif_ps_rem;
void send_gif_frame(int ds, uint8_t const* pixels);
void screen_recording_toggle(uint8_t const* pixels);

extern bool wav_recording;
void send_wav_audio();
void wav_recording_toggle();

// file watching
void file_watch(std::string const& filename);
void file_watch_clear();
void file_watch_check();

extern float pixel_ratio;
extern bool done;
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
void modal_settings();
void modal_about();

constexpr size_t FFT_NUM_SAMPLES = 4096;
void process_sound_samples();

enum
{
    PALETTE_DEFAULT,
    PALETTE_RETRO,
    PALETTE_LOW_CONTRAST,
    PALETTE_HIGH_CONTRAST,

    PALETTE_MAX_PLUS_ONE,
    PALETTE_MIN = PALETTE_DEFAULT,
    PALETTE_MAX = PALETTE_MAX_PLUS_ONE - 1,
};

enum
{
    PGRID_NONE,
    PGRID_NORMAL,
    PGRID_RED,
    PGRID_GREEN,
    PGRID_BLUE,
    PGRID_CYAN,
    PGRID_MAGENTA,
    PGRID_YELLOW,
    PGRID_WHITE,

    PGRID_MAX_PLUS_ONE,
    PGRID_MIN = PGRID_NONE,
    PGRID_MAX = PGRID_MAX_PLUS_ONE - 1,
};

void palette_rgba(int palette, uint8_t x, uint8_t y[4]);
uint8_t* recording_pixels(bool rgba);
int filter_zoom(int f);
int display_filter_zoom();
int recording_filter_zoom();
void recreate_display_texture();
void scalenx(uint8_t* dst, uint8_t const* src, bool rgba);

void load_savedata();
void check_save_savedata(); // save savedata if necessary

// defined in window_data_space.cpp
void symbol_tooltip(
    uint16_t addr, absim::elf_data_symbol_t const& sym, bool prog = false);

void window_disassembly(bool& open);
void window_profiler(bool& open);
void window_display(bool& open);
void window_display_buffer(bool& open);
void window_display_internals(bool& open);
void window_data_space(bool& open);
void window_progmem(bool& open);
void window_simulation(bool& open);
void window_call_stack(bool& open);
void window_symbols(bool& open);
#ifdef ARDENS_LLVM
void window_globals(bool& open);
void window_locals(bool& open);
#endif
void window_fx_data(bool& open);
void window_fx_internals(bool& open);
void window_eeprom(bool& open);
void window_cpu_usage(bool& open);
void window_led(bool& open);
void window_serial(bool& open);
void window_sound(bool& open);
void window_source(bool& open);
