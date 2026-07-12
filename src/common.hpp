#pragma once

#include <filesystem>
#include <memory>
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

#if defined(ARDENS_DISTNAME)
#define ARDENS_TITLE ARDENS_DISTNAME
#elif defined(ARDENS_FLASHCART)
#define ARDENS_TITLE "Ardens Flashcart Player"
#elif defined(ARDENS_PLAYER)
#define ARDENS_TITLE "Ardens Player"
#else
#define ARDENS_TITLE "Ardens"
#endif

constexpr uint32_t AUDIO_FREQ = 16000000 / absim::atmega32u4_t::SOUND_CYCLES;

#if defined(ARDENS_PLATFORM_SOKOL)
// sokol_imgui uses 64-bit texture IDs; wasm pointers are 32-bit, so store the
// full ID explicitly on the Sokol path.
using texture_t = uint64_t;
#else
using texture_t = void*;
#endif

constexpr float CLEAR_R = 0.10f;
constexpr float CLEAR_G = 0.10f;
constexpr float CLEAR_B = 0.10f;

constexpr uint64_t MS_SHOW_TOUCH_CONTROLS = 10000;
struct touch_point_t { float x, y; };
enum { TOUCH_U, TOUCH_D, TOUCH_L, TOUCH_R, TOUCH_A, TOUCH_B };
struct touched_buttons_t { bool btns[6]; };
struct touch_rect_t { float x0, y0, x1, y1; };
touched_buttons_t touched_buttons(); // defined in view_player
touch_rect_t touch_rect(int btn);

struct app_state_t
{
    std::unique_ptr<absim::arduboy_t> emulator = std::make_unique<absim::arduboy_t>();
    std::unique_ptr<absim::arduboy_t> linked_secondary_arduboy;
    absim::local_i2c_transaction_bridge_t linked_i2c_bridge;
    int64_t linked_secondary_arduboy_cycle_offset{};
    uint64_t linked_realtime_debt_ps{};
    bool linked_secondary_input_focus{};

    int display_texture_zoom = -1;
    texture_t display_texture{};
    int linked_secondary_display_texture_zoom = -1;
    texture_t linked_secondary_display_texture{};
    texture_t display_buffer_texture{};
    std::string dropfile_err;
    bool loading_indicator{};
    uint64_t ms_since_start{};
    std::filesystem::path userpath;

    bool first_touch{};
    bool always_touch{};
    uint64_t ms_since_touch{};
    std::unordered_map<uint64_t, touch_point_t> touch_points;

    float pixel_ratio = 1.f;
    bool done{};
    bool layout_done{};
    bool settings_loaded{};
#ifdef __EMSCRIPTEN__
    bool fs_ready{};
#else
    bool fs_ready = true;
#endif

    int profiler_selected_hotspot = -1;
    int disassembly_scroll_addr = -1;
    bool scroll_addr_to_top{};
    int simulation_slowdown = 1000;
    int fx_data_scroll_addr = -1;

    int display_buffer_addr = -1;
    int display_buffer_w = 128;
    int display_buffer_h = 64;
};

struct platform_services_t
{
    void (*destroy_texture)(texture_t){};
    texture_t (*create_texture)(int, int){};
    void (*update_texture)(texture_t, void const*, size_t){};
    void (*texture_scale_linear)(texture_t){};
    void (*texture_scale_nearest)(texture_t){};
    void (*set_clipboard_text)(char const*){};
    void (*send_sound)(){};
    uint64_t (*get_ms_dt)(){};
    float (*pixel_ratio)(){};
    void (*open_url)(char const*){};
    void (*toggle_fullscreen)(){};
    void (*quit)(){};
    void (*set_title)(char const*){};
};

extern app_state_t app;
extern platform_services_t platform_services;

// platform-agnostic functionality (common.cpp)
extern "C" int setparam(char const* name, char const* value);
extern "C" int load_file(
    char const* param, char const* filename, uint8_t const* data, size_t size);
extern "C" int get_led_state(int component);
void init();
void shutdown();
std::filesystem::path app_config_folder();
std::filesystem::path app_config_ini_path();
void frame_logic();
void imgui_content();
bool update_pixel_ratio();
void rescale_style();
void save_screenshot();
void toggle_recording();
void take_snapshot();
std::string timestamped_filename(
    char const* prefix,
    char const* extension);
std::string preferred_title();
void apply_runtime_settings(absim::arduboy_t& a);
void advance_primary(uint64_t ps);
void advance_primary_instr();
void reset_primary_simulation();
bool connect_linked_secondary_arduboy();
void disconnect_linked_secondary_arduboy();
bool linked_secondary_arduboy_connected();
void swap_linked_secondary_arduboy();

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


uint32_t color_for_index(size_t index);
uint32_t darker_color_for_index(size_t index);

void view_debugger();
void view_player();
struct ImDrawList;
struct ImVec2;
void display_with_scanlines(
    ImDrawList* d, ImVec2 const& a, ImVec2 const& b, texture_t texture, int texture_zoom);
void display_with_scanlines(ImDrawList* d, ImVec2 const& a, ImVec2 const& b);
void draw_display_texture(texture_t texture, int texture_zoom);
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
void recreate_display_texture(texture_t& texture, int& texture_zoom);
void recreate_display_texture();
void scalenx(uint8_t* dst, uint8_t const* src, bool rgba);
void update_display_texture(texture_t texture, uint8_t const* src);

std::string savedata_filename();
void load_savedata();
void check_save_savedata(); // save savedata if necessary
void flush_savedata(); // immediately save pending savedata

// defined in window_data_space.cpp
void symbol_tooltip(
    uint16_t addr, absim::elf_data_symbol_t const& sym, bool prog = false);

void window_disassembly(bool& open);
void window_profiler(bool& open);
void window_display(bool& open);
void window_linked_secondary_arduboy(bool& open);
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
void window_savefile(bool& open);
void window_serial(bool& open);
void window_sound(bool& open);
void window_source(bool& open);
