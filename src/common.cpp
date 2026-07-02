#ifndef _SILENCE_CXX17_STRSTREAM_DEPRECATION_WARNING
#define _SILENCE_CXX17_STRSTREAM_DEPRECATION_WARNING 1
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "common.hpp"

#include "absim_strstream.hpp"

#include <fstream>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <ctime>

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <fmt/format.h>

#ifndef __EMSCRIPTEN__
#include <cfgpath.h>
#endif

#include <imgui.h>

#define SOKOL_FETCH_IMPL
#include "sokol/sokol_fetch.h"

#define PROFILING 0

constexpr int SPEEDUP = PROFILING ? 10 : 1;

#ifdef ARDENS_DIST
extern "C" {
#include <distgame.h>
}
#endif

#ifndef ARDENS_NO_DEBUGGER
#include <implot.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include "emscripten-browser-file/emscripten_browser_file.h"
#endif

#if !defined(__EMSCRIPTEN__)
#define ALLOW_SCREENSHOTS 1
#else
#define ALLOW_SCREENSHOTS 1
#endif

#if ALLOW_SCREENSHOTS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#ifdef ARDENS_NO_DEBUGGER
void process_sound_samples() {}
#endif

app_state_t app;
platform_services_t platform_services;

static ImGuiStyle default_style;

#if defined(ARDENS_FLASHCART)
static uint8_t FLASHCART_BUFFER[8 * 1024 * 1024];
static std::string flashcart_error;

static void flashcart_fetch_callback(sfetch_response_t const* r)
{
    if(r->fetched)
    {
        flashcart_error = app.emulator.load_flashcart_zip(
            (uint8_t const*)r->data.ptr, r->data.size);
    }
    else if(r->finished)
    {
        flashcart_error = "Could not load flashcart";
    }
    if(r->finished)
    {
        app.loading_indicator = false;
        if(flashcart_error.empty())
            printf("Flashcart loaded successfully.\n");
        else
            printf("%s\n", flashcart_error.c_str());
    }
}
#endif

#ifdef __EMSCRIPTEN__
void file_download(
    char const* fname,
    char const* download_fname,
    char const* mime_type)
{
    std::ifstream f(fname, std::ios::binary);
    std::vector<char> data(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
    emscripten_browser_file::download(download_fname, mime_type, data.data(), data.size());
}
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

static void default_platform_open_url(char const *url)
{
#ifdef __EMSCRIPTEN__
    EM_ASM(
        { window.open(UTF8ToString($0), "_blank"); },
        url);
#endif
#ifdef _WIN32
    ShellExecuteA(0, NULL, url, NULL, NULL, SW_SHOWNORMAL);
#endif
#if defined(__APPLE__) || defined(__MACH__)
    (void)system(fmt::format("open {}", url).c_str());
#endif
#if defined(__linux__) || defined(__FreeBSD__)
    (void)system(fmt::format("xdg-open {}", url).c_str());
#endif
}

void platform_destroy_texture(texture_t t)
{
    if(platform_services.destroy_texture)
        platform_services.destroy_texture(t);
}

texture_t platform_create_texture(int w, int h)
{
    return platform_services.create_texture ?
        platform_services.create_texture(w, h) :
        nullptr;
}

void platform_update_texture(texture_t t, void const* data, size_t n)
{
    if(platform_services.update_texture)
        platform_services.update_texture(t, data, n);
}

void platform_texture_scale_linear(texture_t t)
{
    if(platform_services.texture_scale_linear)
        platform_services.texture_scale_linear(t);
}

void platform_texture_scale_nearest(texture_t t)
{
    if(platform_services.texture_scale_nearest)
        platform_services.texture_scale_nearest(t);
}

void platform_set_clipboard_text(char const* str)
{
    if(platform_services.set_clipboard_text)
        platform_services.set_clipboard_text(str);
}

void platform_send_sound()
{
    if(platform_services.send_sound)
        platform_services.send_sound();
}

uint64_t platform_get_ms_dt()
{
    return platform_services.get_ms_dt ?
        platform_services.get_ms_dt() :
        0;
}

float platform_pixel_ratio()
{
    return platform_services.pixel_ratio ?
        platform_services.pixel_ratio() :
        1.f;
}

void platform_open_url(char const* url)
{
    if(platform_services.open_url)
        platform_services.open_url(url);
    else
        default_platform_open_url(url);
}

void platform_toggle_fullscreen()
{
    if(platform_services.toggle_fullscreen)
        platform_services.toggle_fullscreen();
}

void platform_quit()
{
    if(platform_services.quit)
        platform_services.quit();
}

void platform_set_title(char const* title)
{
    if(platform_services.set_title)
        platform_services.set_title(title);
}

bool linked_secondary_arduboy_connected()
{
    return app.linked_secondary_arduboy != nullptr;
}

static void set_unpressed_buttons(absim::arduboy_t& a)
{
    auto& cpu = a.core_state.cpu;
    cpu.data[absim::reg::addr::PINB] = absim::reg::bit::PINB::PINB4;
    cpu.data[absim::reg::addr::PINE] = absim::reg::bit::PINE::PINE6;
    cpu.data[absim::reg::addr::PINF] =
        absim::reg::bit::PINF::PINF7 |
        absim::reg::bit::PINF::PINF6 |
        absim::reg::bit::PINF::PINF5 |
        absim::reg::bit::PINF::PINF4;
}

static void set_current_buttons(absim::arduboy_t& a)
{
    // PINF: 4,5,6,7=D,L,R,U
    // PINE: 6=A
    // PINB: 4=B
    uint8_t pinf =
        absim::reg::bit::PINF::PINF7 |
        absim::reg::bit::PINF::PINF6 |
        absim::reg::bit::PINF::PINF5 |
        absim::reg::bit::PINF::PINF4;
    uint8_t pine = absim::reg::bit::PINE::PINE6;
    uint8_t pinb = absim::reg::bit::PINB::PINB4;

    std::array<ImGuiKey, 4> keys =
    {
        ImGuiKey_UpArrow,
        ImGuiKey_RightArrow,
        ImGuiKey_DownArrow,
        ImGuiKey_LeftArrow,
    };
    std::array<int, 4> tkeys =
    {
        TOUCH_U, TOUCH_R, TOUCH_D, TOUCH_L,
    };
    auto touch = touched_buttons();

    std::rotate(keys.begin(), keys.begin() + settings.display_orientation, keys.end());
    std::rotate(tkeys.begin(), tkeys.begin() + settings.display_orientation, tkeys.end());

    if(ImGui::IsKeyDown(keys[0]) || touch.btns[tkeys[0]]) pinf &= ~absim::reg::bit::PINF::PINF7;
    if(ImGui::IsKeyDown(keys[1]) || touch.btns[tkeys[1]]) pinf &= ~absim::reg::bit::PINF::PINF6;
    if(ImGui::IsKeyDown(keys[2]) || touch.btns[tkeys[2]]) pinf &= ~absim::reg::bit::PINF::PINF4;
    if(ImGui::IsKeyDown(keys[3]) || touch.btns[tkeys[3]]) pinf &= ~absim::reg::bit::PINF::PINF5;

    if( ImGui::IsKeyDown(ImGuiKey_A) ||
        ImGui::IsKeyDown(ImGuiKey_Z) ||
        touch.btns[TOUCH_A])
        pine &= ~absim::reg::bit::PINE::PINE6;
    if( ImGui::IsKeyDown(ImGuiKey_B) ||
        ImGui::IsKeyDown(ImGuiKey_S) ||
        ImGui::IsKeyDown(ImGuiKey_X) ||
        touch.btns[TOUCH_B])
        pinb &= ~absim::reg::bit::PINB::PINB4;

    auto& cpu = a.core_state.cpu;
    cpu.data[absim::reg::addr::PINB] = pinb;
    cpu.data[absim::reg::addr::PINE] = pine;
    cpu.data[absim::reg::addr::PINF] = pinf;
}

void apply_runtime_settings(absim::arduboy_t& a)
{
    a.core_state.cpu.adc_nondeterminism = settings.nondeterminism;
    a.profiler_state.frame_bytes_total = 1024;
    a.debugger_state.allow_nonstep_breakpoints =
        a.debugger_state.break_step == 0xffffffff || settings.enable_step_breaks;
    a.program_state.cfg.usb_bus_state =
        settings.usb_connected ?
        absim::USB_BUS_CONNECTED :
        absim::USB_BUS_DISCONNECTED;

    auto& display = a.peripherals.display;
    display.enable_filter = settings.display_auto_filter;
    display.enable_current_limiting = (settings.display_current_modeling != 0);
    display.ref_segment_current = 0.195f;
    switch(settings.display_current_modeling)
    {
    case 0:  display.current_limit_slope = 0.f;   break;
    case 1:  display.current_limit_slope = 0.75f; break;
    case 2:  display.current_limit_slope = 0.45f; break;
    case 3:  display.current_limit_slope = 0.f;   break;
    default: display.current_limit_slope = 0.f;   break;
    }

    switch(settings.fxport)
    {
    case FXPORT_D1:
        a.peripherals.fxport_reg = absim::reg::addr::PORTD;
        a.peripherals.fxport_mask = absim::reg::bit::PORTD::PORTD1;
        break;
    case FXPORT_D2:
        a.peripherals.fxport_reg = absim::reg::addr::PORTD;
        a.peripherals.fxport_mask = absim::reg::bit::PORTD::PORTD2;
        break;
    case FXPORT_E2:
        a.peripherals.fxport_reg = absim::reg::addr::PORTE;
        a.peripherals.fxport_mask = absim::reg::bit::PORTE::PORTE2;
        break;
    default:
        break;
    }

    switch(settings.display)
    {
    case DISPLAY_SSD1306:
        display.type = absim::display_t::SSD1306;
        break;
    case DISPLAY_SSD1309:
        display.type = absim::display_t::SSD1309;
        break;
    case DISPLAY_SH1106:
        display.type = absim::display_t::SH1106;
        break;
    default:
        break;
    }
}

void disconnect_linked_secondary_arduboy()
{
    app.linked_secondary_input_focus = false;
    app.linked_i2c_bridge.disconnect();

    platform_destroy_texture(app.linked_secondary_display_texture);
    app.linked_secondary_display_texture = {};
    app.linked_secondary_display_texture_zoom = -1;
    app.linked_secondary_arduboy.reset();
}

static void preroll_linked_secondary_arduboy()
{
    if(!app.linked_secondary_arduboy) return;

    apply_runtime_settings(*app.linked_secondary_arduboy);
    set_unpressed_buttons(*app.linked_secondary_arduboy);
    app.linked_secondary_arduboy->debugger_state.paused = false;
    app.linked_i2c_bridge.update_bus_lines();
    app.linked_secondary_arduboy->advance(50ull * 1000000000ull);
    app.linked_i2c_bridge.update_bus_lines();
}

static bool load_linked_secondary_arduboy(bool preroll)
{
    auto& primary = app.emulator;
    if(!primary.core_state.cpu.decoded || primary.program_state.prog_filedata.empty())
        return false;

    auto secondary = std::make_unique<absim::arduboy_t>();
    secondary->program_state.cfg = primary.program_state.cfg;
    absim::istrstream f(
        (char const*)primary.program_state.prog_filedata.data(),
        (int)primary.program_state.prog_filedata.size());
    std::string err = secondary->load_file(primary.program_state.prog_filename.c_str(), f);
    if(!err.empty())
    {
        app.dropfile_err = err;
        return false;
    }

    app.linked_secondary_arduboy = std::move(secondary);
    app.linked_i2c_bridge.connect({ &primary, app.linked_secondary_arduboy.get() });
    app.linked_i2c_bridge.update_bus_lines();
    if(preroll)
        preroll_linked_secondary_arduboy();
    return true;
}

bool connect_linked_secondary_arduboy()
{
    disconnect_linked_secondary_arduboy();
    return load_linked_secondary_arduboy(false);
}

static void reset_linked_secondary_arduboy()
{
    if(!app.linked_secondary_arduboy) return;

    app.linked_i2c_bridge.disconnect();
    app.linked_secondary_arduboy.reset();

    load_linked_secondary_arduboy(true);
}

void reset_primary_simulation()
{
    apply_runtime_settings(app.emulator);
    app.emulator.reset();
    load_savedata();
    reset_linked_secondary_arduboy();
}

static void advance_linked_secondary(uint64_t ps)
{
    if(!app.linked_secondary_arduboy || ps == 0) return;

    bool prev_paused = app.linked_secondary_arduboy->debugger_state.paused;
    app.linked_secondary_arduboy->debugger_state.paused = false;
    apply_runtime_settings(*app.linked_secondary_arduboy);
    if(!app.linked_secondary_input_focus)
        set_unpressed_buttons(*app.linked_secondary_arduboy);
    app.linked_secondary_arduboy->advance(ps);
    app.linked_secondary_arduboy->debugger_state.paused = prev_paused;
}

void advance_primary(uint64_t ps)
{
    if(!app.linked_secondary_arduboy)
    {
        app.emulator.advance(ps);
        return;
    }

    constexpr uint64_t LINK_UPDATE_PS = 10ull * 1000000ull;
    while(ps > 0 && !app.emulator.debugger_state.paused)
    {
        uint64_t chunk = std::min<uint64_t>(ps, LINK_UPDATE_PS);
        uint64_t before = app.emulator.core_state.cpu.cycle_count;

        app.linked_i2c_bridge.update_bus_lines();
        app.emulator.advance(chunk);
        app.linked_i2c_bridge.update_bus_lines();

        uint64_t cycles = app.emulator.core_state.cpu.cycle_count - before;
        advance_linked_secondary(cycles * absim::arduboy_t::CYCLE_PS);
        app.linked_i2c_bridge.update_bus_lines();

        if(cycles == 0 && chunk >= ps)
            break;
        ps -= chunk;
    }
}

void advance_primary_instr()
{
    uint64_t before = app.emulator.core_state.cpu.cycle_count;
    app.emulator.advance_instr();
    uint64_t cycles = app.emulator.core_state.cpu.cycle_count - before;
    if(app.linked_secondary_arduboy)
    {
        app.linked_i2c_bridge.update_bus_lines();
        advance_linked_secondary(cycles * absim::arduboy_t::CYCLE_PS);
        app.linked_i2c_bridge.update_bus_lines();
    }
}

extern "C" int load_file(
    char const* param, char const* filename, uint8_t const* data, size_t size)
{
    absim::istrstream f((char const*)data, size);
    bool save = !strcmp(param, "save");
    if(!save)
        disconnect_linked_secondary_arduboy();
    app.dropfile_err = app.emulator.load_file(filename, f, save);
    autoset_from_device_type();
    if(app.dropfile_err.empty())
    {
        load_savedata();
#ifndef ARDENS_DIST
        if(!save) file_watch(filename);
#endif
    }
    return 0;
}

extern "C" int get_led_state(int component)
{
    if(!app.emulator.core_state.cpu.decoded) return 0;

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    app.emulator.core_state.cpu.led_rgb(r, g, b);

    switch(component)
    {
    case 0: return app.emulator.core_state.cpu.led_tx();
    case 1: return app.emulator.core_state.cpu.led_rx();
    case 2: return r;
    case 3: return g;
    case 4: return b;
    default: return 0;
    }
}

#ifdef __EMSCRIPTEN__
static void install_js_api()
{
    EM_ASM({
        var root = (typeof globalThis !== 'undefined') ? globalThis : window;
        root['Ardens'] = Module;
        Module['getLedState'] = function() {
            var get = function(component) {
                var fn = Module['_get_led_state'];
                if(typeof fn === 'function')
                    return fn(component) | 0;
                if(typeof Module['ccall'] === 'function')
                {
                    var args = [];
                    args[0] = 'get_led_state';
                    args[1] = 'number';
                    args[2] = ['number'];
                    args[3] = [component];
                    return Module['ccall'].apply(null, args) | 0;
                }
                return 0;
            };
            var state = {};
            state['tx'] = get(0);
            state['rx'] = get(1);
            state['r'] = get(2);
            state['g'] = get(3);
            state['b'] = get(4);
            return state;
        };
    });
}
#else
static void install_js_api() {}
#endif

float volume_gain()
{
    float gain = 1.f / 32768;
    float f = float(settings.volume) * 1.f / 200;
    f = powf(f, 1.5f) * 5.f;
    gain *= f;
    return gain;
}

bool ends_with(std::string const& str, std::string const& end)
{
    if(str.size() < end.size()) return false;
    return str.substr(str.size() - end.size()) == end;
}

static bool string_equals_ignore_case(char const* a, char const* b)
{
    if(!a || !b) return false;
    while(*a && *b)
    {
        unsigned char ca = static_cast<unsigned char>(*a++);
        unsigned char cb = static_cast<unsigned char>(*b++);
        if(std::tolower(ca) != std::tolower(cb)) return false;
    }
    return *a == *b;
}

static bool parse_int(char const* value, int& result)
{
    if(!value || !*value) return false;
    errno = 0;
    char* end = nullptr;
    long parsed = strtol(value, &end, 10);
    if(errno || end == value || *end) return false;
    result = static_cast<int>(parsed);
    return true;
}

static bool parse_clamped_int(char const* value, int min_value, int max_value, int& result)
{
    int parsed = 0;
    if(!parse_int(value, parsed)) return false;
    result = std::clamp<int>(parsed, min_value, max_value);
    return true;
}

static bool parse_bool(char const* value, bool& result)
{
    int parsed = 0;
    if(parse_int(value, parsed))
    {
        result = parsed != 0;
        return true;
    }
    if(string_equals_ignore_case(value, "true") ||
        string_equals_ignore_case(value, "yes") ||
        string_equals_ignore_case(value, "on"))
    {
        result = true;
        return true;
    }
    if(string_equals_ignore_case(value, "false") ||
        string_equals_ignore_case(value, "no") ||
        string_equals_ignore_case(value, "off"))
    {
        result = false;
        return true;
    }
    return false;
}

static bool parse_grid(char const* value, int& result)
{
    if(string_equals_ignore_case(value, "none")) result = PGRID_NONE;
    else if(string_equals_ignore_case(value, "normal")) result = PGRID_NORMAL;
    else if(string_equals_ignore_case(value, "red")) result = PGRID_RED;
    else if(string_equals_ignore_case(value, "green")) result = PGRID_GREEN;
    else if(string_equals_ignore_case(value, "blue")) result = PGRID_BLUE;
    else if(string_equals_ignore_case(value, "cyan")) result = PGRID_CYAN;
    else if(string_equals_ignore_case(value, "magenta")) result = PGRID_MAGENTA;
    else if(string_equals_ignore_case(value, "yellow")) result = PGRID_YELLOW;
    else if(string_equals_ignore_case(value, "white")) result = PGRID_WHITE;
    else return parse_clamped_int(value, PGRID_MIN, PGRID_MAX, result);
    return true;
}

static bool parse_palette(char const* value, int& result)
{
    if(string_equals_ignore_case(value, "default")) result = PALETTE_DEFAULT;
    else if(string_equals_ignore_case(value, "retro")) result = PALETTE_RETRO;
    else if(string_equals_ignore_case(value, "lowcontrast")) result = PALETTE_LOW_CONTRAST;
    else if(string_equals_ignore_case(value, "highcontrast")) result = PALETTE_HIGH_CONTRAST;
    else return parse_clamped_int(value, PALETTE_MIN, PALETTE_MAX, result);
    return true;
}

static bool parse_filter(char const* value, int& result)
{
    if(string_equals_ignore_case(value, "none")) result = FILTER_NONE;
    else if(string_equals_ignore_case(value, "scale2x")) result = FILTER_SCALE2X;
    else if(string_equals_ignore_case(value, "scale3x")) result = FILTER_SCALE3X;
    else if(string_equals_ignore_case(value, "scale4x")) result = FILTER_SCALE4X;
    else if(string_equals_ignore_case(value, "hq2x")) result = FILTER_HQ2X;
    else if(string_equals_ignore_case(value, "hq3x")) result = FILTER_HQ3X;
    else if(string_equals_ignore_case(value, "hq4x")) result = FILTER_HQ4X;
    else return parse_clamped_int(value, FILTER_MIN, FILTER_MAX, result);
    return true;
}

static bool parse_orientation(char const* value, int& result)
{
    if(string_equals_ignore_case(value, "0") || string_equals_ignore_case(value, "normal"))
        result = 0;
    else if(string_equals_ignore_case(value, "90") ||
        string_equals_ignore_case(value, "cw") ||
        string_equals_ignore_case(value, "cw90"))
        result = 1;
    else if(string_equals_ignore_case(value, "180") || string_equals_ignore_case(value, "flip"))
        result = 2;
    else if(string_equals_ignore_case(value, "270") ||
        string_equals_ignore_case(value, "ccw") ||
        string_equals_ignore_case(value, "ccw90"))
        result = 3;
    else return parse_clamped_int(value, 0, 3, result);
    return true;
}

static bool parse_current_model(char const* value, int& result)
{
    if(parse_clamped_int(value, 0, 3, result))
        return true;
    if(string_equals_ignore_case(value, "true") ||
        string_equals_ignore_case(value, "yes") ||
        string_equals_ignore_case(value, "on"))
    {
        result = 1;
        return true;
    }
    if(string_equals_ignore_case(value, "false") ||
        string_equals_ignore_case(value, "no") ||
        string_equals_ignore_case(value, "off"))
    {
        result = 0;
        return true;
    }
    if(string_equals_ignore_case(value, "subtle")) result = 1;
    else if(string_equals_ignore_case(value, "normal")) result = 2;
    else if(string_equals_ignore_case(value, "exaggerated")) result = 3;
    else return false;
    return true;
}

static bool parse_fxport(char const* value, int& result)
{
    if(string_equals_ignore_case(value, "d1") || string_equals_ignore_case(value, "fx"))
        result = FXPORT_D1;
    else if(string_equals_ignore_case(value, "d2") || string_equals_ignore_case(value, "fxdevkit"))
        result = FXPORT_D2;
    else if(string_equals_ignore_case(value, "e2") || string_equals_ignore_case(value, "mini"))
        result = FXPORT_E2;
    else return parse_clamped_int(value, 0, FXPORT_NUM - 1, result);
    return true;
}

static bool parse_display(char const* value, int& result)
{
    if(string_equals_ignore_case(value, "ssd1306")) result = DISPLAY_SSD1306;
    else if(string_equals_ignore_case(value, "ssd1309")) result = DISPLAY_SSD1309;
    else if(string_equals_ignore_case(value, "sh1106")) result = DISPLAY_SH1106;
    else return parse_clamped_int(value, 0, DISPLAY_NUM - 1, result);
    return true;
}

extern "C" int setparam(char const* name, char const* value)
{
    if(!name || !value) return 0;
    int r = 0;
    if(!strcmp(name, "z"))
    {
        bool parsed = false;
        if(parse_bool(value, parsed))
        {
            settings.fullzoom = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "g") || !strcmp(name, "grid"))
    {
        int parsed = 0;
        if(parse_grid(value, parsed))
        {
            settings.display_pixel_grid = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "p") || !strcmp(name, "palette"))
    {
        int parsed = 0;
        if(parse_palette(value, parsed))
        {
            settings.display_palette = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "af") || !strcmp(name, "autofilter"))
    {
        bool parsed = false;
        if(parse_bool(value, parsed))
        {
            settings.display_auto_filter = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "f") || !strcmp(name, "filter"))
    {
        int parsed = 0;
        if(parse_filter(value, parsed))
        {
            settings.display_filtering = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "ds") || !strcmp(name, "downsample"))
    {
        int parsed = 0;
        if(parse_clamped_int(value, 1, 4, parsed))
        {
            settings.display_downsample = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "ori") || !strcmp(name, "orientation"))
    {
        int parsed = 0;
        if(parse_orientation(value, parsed))
        {
            settings.display_orientation = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "v") || !strcmp(name, "volume"))
    {
        int parsed = 0;
        if(parse_clamped_int(value, 0, 200, parsed))
        {
            settings.volume = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "i") || !strcmp(name, "intscale"))
    {
        bool parsed = false;
        if(parse_bool(value, parsed))
        {
            settings.display_integer_scale = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "c") || !strcmp(name, "current"))
    {
        int parsed = 0;
        if(parse_current_model(value, parsed))
        {
            settings.display_current_modeling = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "fxport"))
    {
        int parsed = 0;
        if(parse_fxport(value, parsed))
        {
            settings.fxport = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "display"))
    {
        int parsed = 0;
        if(parse_display(value, parsed))
        {
            settings.display = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "usb") || !strcmp(name, "usb_connected"))
    {
        bool parsed = false;
        if(parse_bool(value, parsed))
        {
            settings.usb_connected = parsed;
            update_settings();
        }
        r = 1;
    }
    else if(!strcmp(name, "touch"))
    {
        bool parsed = false;
        if(parse_bool(value, parsed))
            app.always_touch = parsed;
        r = 1;
    }
    else if(!strcmp(name, "loading"))
    {
        bool parsed = false;
        if(parse_bool(value, parsed))
            app.loading_indicator = parsed;
        r = 1;
    }
    return r;
}

extern "C" void postsyncfs()
{
    app.fs_ready = true;
    load_savedata();
}

bool update_pixel_ratio()
{
    float ratio = platform_pixel_ratio();
    if(!(ratio > 0.f))
        ratio = 1.f;
    ratio *= (settings.uiscale * 0.25f + 0.5f);
    bool changed = (ratio != app.pixel_ratio);
    app.pixel_ratio = ratio;
    return changed;
}

static float ui_scale_factor()
{
    return settings.uiscale * 0.25f + 0.5f;
}

void rescale_style()
{
    float const ui_scale = ui_scale_factor();
    float const dpi_scale = app.pixel_ratio / ui_scale;
    auto& style = ImGui::GetStyle();
    style = default_style;
    style.ScaleAllSizes(app.pixel_ratio);
    style.FontSizeBase = 13.f;
    style.FontScaleMain = ui_scale;
    style.FontScaleDpi = dpi_scale > 0.f ? dpi_scale : 1.f;
}

void shutdown()
{
    flush_savedata();
    sfetch_shutdown();
#ifndef ARDENS_NO_DEBUGGER
    ImPlot::DestroyContext();
#endif
}

void init()
{
    install_js_api();

    printf(
        "Ardens "
#if defined(ARDENS_FLASHCART)
        "Flashcart Player "
#elif defined(ARDENS_PLAYER)
        "Player "
#endif
        ARDENS_VERSION " by Peter Brown\n");

    {
        sfetch_desc_t desc{};
        sfetch_setup(&desc);
    }

    ImGuiIO& io = ImGui::GetIO();

#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.mkdir('/offline');
    FS.mount(IDBFS, {}, '/offline');
    FS.syncfs(true, function(err) { ccall('postsyncfs', 'v'); });
    );
    io.IniFilename = "/offline/Ardens.ini";
    app.userpath = "/offline";
#else
    {
        static char ini_path[1024];
        get_user_config_folder(ini_path, sizeof(ini_path), "Ardens");
        app.userpath = ini_path;
        std::error_code ec;
        std::filesystem::create_directories(app.userpath, ec);
        strncpy(
            ini_path,
            (app.userpath / "Ardens.ini").generic_string().c_str(),
            sizeof(ini_path));
        io.IniFilename = ini_path;
    }
#endif

    printf("Data path: %s\n", app.userpath.generic_string().c_str());

    app.emulator.peripherals.display.type = absim::display_t::SSD1306;

#ifndef ARDENS_NO_DEBUGGER
    ImPlot::CreateContext();
#endif

#ifndef ARDENS_NO_SAVED_SETTINGS
    init_settings();
#endif

#if !defined(__EMSCRIPTEN__) && !defined(ARDENS_NO_SAVED_SETTINGS)
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
    app.settings_loaded = true;
#endif

#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif

#if defined(__EMSCRIPTEN__) || defined(ARDENS_NO_SAVED_SETTINGS)
    io.IniFilename = nullptr;
#endif

    ImGui::StyleColorsDark();
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    style.Colors[ImGuiCol_PopupBg].w = 1.0f;
    style.AntiAliasedFill = true;
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;

    default_style = style;

    app.emulator.peripherals.fx.erase_all_data();
    app.emulator.core_state.cpu.adc_nondeterminism = settings.nondeterminism;
    apply_runtime_settings(app.emulator);
    app.emulator.reset();
    app.emulator.peripherals.fx.set_empty_page_range();

    app.ms_since_start = 0;
    app.ms_since_touch = MS_SHOW_TOUCH_CONTROLS;

#ifdef ARDENS_DIST
    load_file("", "game.arduboy", game_arduboy, game_arduboy_size);
#endif

#if defined(ARDENS_FLASHCART)
    {
        sfetch_request_t req{};
        req.buffer.ptr = FLASHCART_BUFFER;
        req.buffer.size = sizeof(FLASHCART_BUFFER);
        req.path = "flashcart.zip";
        req.callback = flashcart_fetch_callback;
        sfetch_send(&req);
        app.loading_indicator = true;
    }
#endif
}

void save_screenshot()
{
    std::string fname = timestamped_filename("screenshot", "png");
    int z = recording_filter_zoom();
    int w = 128 * z;
    int h = 64 * z;
    int stride = 128 * 4 * z;
    if(settings.recording_orientation & 1)
    {
        stride /= 2;
        std::swap(w, h);
    }
#ifdef __EMSCRIPTEN__
    stbi_write_png("screenshot.png", w, h, 4, recording_pixels(true), stride);
    file_download("screenshot.png", fname.c_str(), "image/x-png");
#else
    stbi_write_png(fname.c_str(), w, h, 4, recording_pixels(true), stride);
#endif
}

void toggle_recording()
{
    screen_recording_toggle(recording_pixels(false));
    if(wav_recording || settings.record_wav)
        wav_recording_toggle();
}

void take_snapshot()
{
#ifndef ARDENS_NO_SNAPSHOTS
    std::string fname = timestamped_filename("ardens", "snapshot");
#ifdef __EMSCRIPTEN__
    std::ofstream f("ardens.snapshot", std::ios::binary);
    if(app.emulator.save_snapshot(f))
    {
        f.close();
        file_download("ardens.snapshot", fname.c_str(), "application/octet-stream");
    }
#else
    std::ofstream f(fname, std::ios::binary);
    app.emulator.save_snapshot(f);
#endif
#endif
}

std::string timestamped_filename(char const* prefix, char const* extension)
{
    char timestamp[32];
    time_t rawtime;
    struct tm* ti;
    time(&rawtime);
    ti = localtime(&rawtime);
    (void)snprintf(timestamp, sizeof(timestamp),
        "%04d%02d%02d%02d%02d%02d",
        ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
        ti->tm_hour + 1, ti->tm_min, ti->tm_sec);
    return fmt::format("{}_{}.{}", prefix, timestamp, extension);
}

std::string preferred_title()
{
#ifdef ARDENS_DIST
    if(!app.emulator.program_state.title.empty())
        return app.emulator.program_state.title;
#endif
    return ARDENS_TITLE;
}

void frame_logic()
{
    sfetch_dowork();

    ImGuiIO& io = ImGui::GetIO();

    app.emulator.core_state.cpu.adc_nondeterminism = settings.nondeterminism;
    if(!app.touch_points.empty())
        app.ms_since_touch = 0;

#ifndef ARDENS_DIST
    file_watch_check();
#endif

#ifdef __EMSCRIPTEN__
    if(app.done) emscripten_cancel_main_loop();
#endif

    // advance simulation
    uint64_t dt = platform_get_ms_dt();
    app.ms_since_start += dt;
    app.ms_since_touch += dt;
    if(dt > 100) dt = 100;
    if(app.emulator.core_state.cpu.decoded && !app.emulator.debugger_state.paused)
    {
        if(!ImGui::GetIO().WantCaptureKeyboard)
        {
            bool input_to_secondary =
                !settings.fullzoom &&
                settings.open_linked_secondary_arduboy &&
                app.linked_secondary_input_focus &&
                app.linked_secondary_arduboy != nullptr;

            if(input_to_secondary)
            {
                set_unpressed_buttons(app.emulator);
                set_current_buttons(*app.linked_secondary_arduboy);
            }
            else
            {
                set_current_buttons(app.emulator);
                if(app.linked_secondary_arduboy)
                    set_unpressed_buttons(*app.linked_secondary_arduboy);
            }
        }

        bool prev_paused = app.emulator.debugger_state.paused;
        apply_runtime_settings(app.emulator);

        app.emulator.core_state.cpu.enabled_autobreaks.reset();
#ifndef ARDENS_NO_GUI
        app.emulator.core_state.cpu.enabled_autobreaks.set(absim::AB_BREAK);
        for(int i = 1; i < absim::AB_NUM; ++i)
            if(settings.ab.index(i))
                app.emulator.core_state.cpu.enabled_autobreaks.set(i);
#endif

        constexpr uint64_t MS_TO_PS = 1000000000ull;
        uint64_t dtps = dt * MS_TO_PS * 1000 / app.simulation_slowdown;
        if(gif_recording)
        {
            constexpr uint64_t DT_20_MS = 20 * MS_TO_PS;
            uint64_t ps = DT_20_MS - gif_ps_rem;
            while(dtps >= ps)
            {
                advance_primary(ps);
                send_gif_frame(2, recording_pixels(false));
                dtps -= ps;
                ps = DT_20_MS;
                gif_ps_rem = 0;
            }
            gif_ps_rem += dtps;
        }
        if(dtps > 0)
            advance_primary(dtps * SPEEDUP);

        check_save_savedata();

        if(app.emulator.debugger_state.paused && !prev_paused)
            app.disassembly_scroll_addr = app.emulator.core_state.cpu.pc * 2;
        //if(!settings.enable_stack_breaks)
        //    app.emulator.core_state.cpu.stack_overflow = false;

        // consume sound buffer
        send_wav_audio();
        process_sound_samples();
#if !PROFILING
        if(!app.emulator.core_state.cpu.sound_buffer.empty() && app.simulation_slowdown == 1000)
            platform_send_sound();
#endif
        app.emulator.core_state.cpu.sound_buffer.clear();
    }
    else
    {
        app.emulator.debugger_state.break_step = 0xffffffff;
    }

    // update framebuffer texture
    if(app.emulator.core_state.cpu.decoded)
    {
        recreate_display_texture();
        update_display_texture(
            app.display_texture,
            app.emulator.peripherals.display.filtered_pixels.data());

#if ALLOW_SCREENSHOTS
        if(app.emulator.core_state.cpu.decoded && ImGui::IsKeyPressed(ImGuiKey_F4, false))
            take_snapshot();
        if(app.emulator.core_state.cpu.decoded && ImGui::IsKeyPressed(ImGuiKey_F2, false))
            save_screenshot();
        if(app.emulator.core_state.cpu.decoded && ImGui::IsKeyPressed(ImGuiKey_F3, false))
            toggle_recording();
#endif
    }

#if ARDENS_PLAYER && !defined(__EMSCRIPTEN__)
    if(ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        platform_quit();
#endif

    if(!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_P, false))
    {
        settings.fullzoom = !settings.fullzoom;
        update_settings();
    }

#ifndef ARDENS_NO_DEBUGGER
    if(ImGui::IsKeyPressed(ImGuiKey_F5, false))
        app.emulator.debugger_state.paused = !app.emulator.debugger_state.paused;
#endif
    if(ImGui::IsKeyPressed(ImGuiKey_F8, false))
    {
        reset_primary_simulation();
    }

    if(ImGui::IsKeyPressed(ImGuiKey_F11, false))
        platform_toggle_fullscreen();

#ifdef ARDENS_OS_MACOS
    if(ImGui::GetIO().KeySuper && ImGui::IsKeyPressed(ImGuiKey_Q))
        platform_quit();
#endif

    if(!gif_recording && ImGui::IsKeyPressed(ImGuiKey_R, false))
    {
        settings.display_orientation = (settings.display_orientation + 1) % 4;
        update_settings();
    }

    if(update_pixel_ratio())
        rescale_style();
}

void imgui_content()
{
    ImGuiIO& io = ImGui::GetIO();

    if(!app.emulator.core_state.cpu.decoded)
    {
        auto* d = ImGui::GetBackgroundDrawList();
        auto size = ImGui::GetMainViewport()->Size;
        float w = 100.f * app.pixel_ratio;
        float pad = 25.f * app.pixel_ratio;
        w = std::min(w, size.x - pad);
        w = std::min(w, size.y - pad);
        w = std::max(w, pad);
        float w2 = w * 0.5f;
        float cx = size.x * 0.5f;
        float cy = size.y * 0.5f;
        constexpr uint8_t shade = 200;
        auto color = IM_COL32(shade, shade, shade, 255);
        float thickness = w * 0.04f;
        if(app.loading_indicator)
        {
            for(int i = 0; i < 12; ++i)
            {
                float tx = cosf(3.1415926536f / 6 * i) * 0.5f;
                float ty = sinf(3.1415926536f / 6 * i) * 0.5f;
                int ti = (int)round(fmod(ImGui::GetTime(), 1.0) * 12);
                if(ti > 11) ti = 11;
                if(ti < 0) ti = 0;
                ti -= i;
                if(ti < 0) ti += 12;
                int tf = (int)roundf((float)(11 - ti - 6) * (255.f / 5));
                if(tf < 0) tf = 0;
                d->AddCircleFilled(
                    { cx + tx * w2, cy + ty * w2 },
                    thickness,
                    IM_COL32(shade, shade, shade, tf));
            }
        }
#if defined(ARDENS_FLASHCART)
        else if(!flashcart_error.empty())
        {
            auto const color = IM_COL32(255, 100, 100, 255);
            float const t = thickness * 3.f;
            d->AddLine(
                { cx - w2, cy - w2 },
                { cx + w2, cy + w2 },
                color, t);
            d->AddLine(
                { cx - w2, cy + w2 },
                { cx + w2, cy - w2 },
                color, t);
        }
#endif
        else
        {
#if !defined(ARDENS_FLASHCART)
            d->AddRect(
                { cx - w2, cy - w2 },
                { cx + w2, cy + w2 },
                color,
                10.f * app.pixel_ratio,
                0,
                thickness);
            d->AddLine(
                { cx, cy - w2 * 0.5f },
                { cx, cy },
                color,
                thickness);
            d->AddTriangle(
                { cx - w2 * 0.5f, cy },
                { cx + w2 * 0.5f, cy },
                { cx, cy + w2 * 0.5f },
                color,
                thickness);
#endif
        }
    }

#ifndef ARDENS_NO_DEBUGGER
    if(settings.fullzoom)
        view_player();
    else
        view_debugger();
#else
    view_player();
#endif

#ifndef ARDENS_NO_GUI
    modal_settings();
#endif

#ifndef ARDENS_NO_GUI
    if(!app.dropfile_err.empty() && !ImGui::IsPopupOpen("File Load Error"))
        ImGui::OpenPopup("File Load Error");

    ImGui::SetNextWindowSize({ 500 * app.pixel_ratio, 0 });
    if(ImGui::BeginPopupModal("File Load Error", NULL,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(app.dropfile_err.c_str());
        ImGui::PopTextWrapPos();
        if(ImGui::Button("OK", ImVec2(120 * app.pixel_ratio, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Enter) ||
            app.dropfile_err.empty())
        {
            app.dropfile_err.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
#endif

#ifndef ARDENS_NO_GUI
    if(app.emulator.core_state.cpu.should_autobreak_gui())
        ImGui::OpenPopup("Auto-Break");

    static std::array<char const*, absim::AB_NUM> const AB_REASONS =
    {
        "BREAK Instruction",
        "Stack Overflow",
        "Null Dereference: a load/store was executed at address 0x0000.",
        "Null-Relative Dereference: a load/store with displacement (ldd/std) was executed with a null base pointer.",
        "Out-of-bounds Dereference: a load/store was executed at an address outside of RAM bounds.",
        "EEPROM Out-of-bounds: EEPROM was accessed at an address greater than 0x3ff.",
        "Out-of-bounds Indirect Jump: an indirect jump (ijmp) was executed to an address outside of program memory.",
        "Out-of-bounds PC: the PC is now pointing past the last byte of program memory.",
        "Unknown Instruction: encountered an instruction that is invalid or currently unsupported.",
        "SPI Write Collision: attempted to write to SPI data register before SPI was ready.",
        "FX Busy: attempted to execute FX command while flash chip was busy."
    };

    ImGui::SetNextWindowSize({ 300 * app.pixel_ratio, 0 });
    if(ImGui::BeginPopupModal("Auto-Break", NULL,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings))
    {
        size_t ab = 0;
        auto mask = app.emulator.core_state.cpu.autobreaks & app.emulator.core_state.cpu.enabled_autobreaks;
        for(size_t i = 1; i < mask.size(); ++i)
        {
            if(mask.test(i))
            {
                ab = i;
                break;
            }
        }

        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted("A runtime error was caught.");
        ImGui::Separator();
        ImGui::TextUnformatted(AB_REASONS[ab]);
        ImGui::PopTextWrapPos();
        if(ImGui::Button("OK", ImVec2(120 * app.pixel_ratio, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            app.emulator.core_state.cpu.autobreaks = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
#endif

#ifndef ARDENS_NO_GUI
    modal_about();
#endif

#if PROFILING
        ImGui::ShowMetricsWindow();
#endif

    //ImGui::ShowDemoWindow();
}
