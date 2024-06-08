#include "common.hpp"

#include <fstream>
#include <strstream>
#include <algorithm>

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <fmt/format.h>

#ifndef __EMSCRIPTEN__
#include <cfgpath.h>
#endif

#include <imgui.h>

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

#ifndef ARDENS_NO_GUI
extern unsigned char const ProggyVector[198188];
#endif

std::filesystem::path userpath;

texture_t display_texture{};
texture_t display_buffer_texture{};
std::unique_ptr<absim::arduboy_t> arduboy;

int profiler_selected_hotspot = -1;
int disassembly_scroll_addr = -1;
bool scroll_addr_to_top = false;
int simulation_slowdown = 1000;
int fx_data_scroll_addr = -1;

int display_buffer_addr = -1;
int display_buffer_w = 128;
int display_buffer_h = 64;

static ImGuiStyle default_style;

float pixel_ratio = 1.f;
std::string dropfile_err;
bool loading_indicator = false;
uint64_t ms_since_start;

uint64_t ms_since_touch;
bool first_touch = false;
std::unordered_map<uintptr_t, touch_point_t> touch_points;

bool done = false;
bool layout_done = false;
bool settings_loaded = false;
#ifdef __EMSCRIPTEN__
bool fs_ready = false;
#else
bool fs_ready = true;
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

#ifndef ARDENS_PLATFORM_SDL
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif
void platform_open_url(char const *url)
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
#endif

extern "C" int load_file(
    char const* param, char const* filename, uint8_t const* data, size_t size)
{
    std::istrstream f((char const*)data, size);
    bool save = !strcmp(param, "save");
    dropfile_err = arduboy->load_file(filename, f, save);
    autoset_from_device_type();
    if(dropfile_err.empty())
    {
        load_savedata();
#ifndef ARDENS_DIST
        if(!save) file_watch(filename);
#endif
    }
    return 0;
}

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

extern "C" int setparam(char const* name, char const* value)
{
    if(!name || !value) return 0;
    int nvalue = atoi(value);
    bool bvalue = (nvalue != 0);
    int r = 0;
    if(!strcmp(name, "z"))
    {
        settings.fullzoom = bvalue;
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "g") || !strcmp(name, "grid"))
    {
        if(!strcmp(value, "none"))
            settings.display_pixel_grid = PGRID_NONE;
        else if(!strcmp(value, "normal"))
            settings.display_pixel_grid = PGRID_NORMAL;
        else if(!strcmp(value, "red"))
            settings.display_pixel_grid = PGRID_RED;
        else if(!strcmp(value, "green"))
            settings.display_pixel_grid = PGRID_GREEN;
        else if(!strcmp(value, "blue"))
            settings.display_pixel_grid = PGRID_BLUE;
        else if(!strcmp(value, "cyan"))
            settings.display_pixel_grid = PGRID_CYAN;
        else if(!strcmp(value, "magenta"))
            settings.display_pixel_grid = PGRID_MAGENTA;
        else if(!strcmp(value, "yellow"))
            settings.display_pixel_grid = PGRID_YELLOW;
        else if(!strcmp(value, "white"))
            settings.display_pixel_grid = PGRID_WHITE;
        else
            settings.display_pixel_grid = std::clamp<int>(nvalue, PGRID_MIN, PGRID_MAX);
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "p") || !strcmp(name, "palette"))
    {
        if(!strcmp(value, "default"))
            settings.display_palette = PALETTE_DEFAULT;
        else if(!strcmp(value, "retro"))
            settings.display_palette = PALETTE_RETRO;
        else if(!strcmp(value, "lowcontrast"))
            settings.display_palette = PALETTE_LOW_CONTRAST;
        else if(!strcmp(value, "highcontrast"))
            settings.display_palette = PALETTE_HIGH_CONTRAST;
        else
            settings.display_palette = std::clamp<int>(nvalue, PALETTE_MIN, PALETTE_MAX);
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "af") || !strcmp(name, "autofilter"))
    {
        settings.display_auto_filter = bvalue;
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "f") || !strcmp(name, "filter"))
    {
        if(!strcmp(value, "none"))
            settings.display_filtering = FILTER_NONE;
        else if(!strcmp(value, "scale2x"))
            settings.display_filtering = FILTER_SCALE2X;
        else if(!strcmp(value, "scale3x"))
            settings.display_filtering = FILTER_SCALE3X;
        else if(!strcmp(value, "scale4x"))
            settings.display_filtering = FILTER_SCALE4X;
        else if(!strcmp(value, "hq2x"))
            settings.display_filtering = FILTER_HQ2X;
        else if(!strcmp(value, "hq3x"))
            settings.display_filtering = FILTER_HQ3X;
        else if(!strcmp(value, "hq4x"))
            settings.display_filtering = FILTER_HQ4X;
        else
            settings.display_filtering = std::clamp<int>(nvalue, FILTER_MIN, FILTER_MAX);
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "ds") || !strcmp(name, "downsample"))
    {
        settings.display_downsample = std::clamp<int>(nvalue, 1, 4);
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "ori") || !strcmp(name, "orientation"))
    {
        if(!strcmp(value, "0") || !strcmp(value, "normal"))
            settings.display_orientation = 0;
        else if(!strcmp(value, "90") || !strcmp(value, "cw") || !strcmp(value, "cw90"))
            settings.display_orientation = 1;
        else if(!strcmp(value, "180") || !strcmp(value, "flip"))
            settings.display_orientation = 2;
        else if(!strcmp(value, "270") || !strcmp(value, "ccw") || !strcmp(value, "ccw90"))
            settings.display_orientation = 3;
        else
            settings.display_orientation = std::clamp<int>(nvalue, 0, 3);
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "v") || !strcmp(name, "volume"))
    {
        settings.volume = std::clamp<int>(nvalue, 0, 200);
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "i") || !strcmp(name, "intscale"))
    {
        settings.display_integer_scale = bvalue;
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "c") || !strcmp(name, "current"))
    {
        settings.display_current_modeling = std::clamp<int>(nvalue, 0, 3);;
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "fxport"))
    {
        if(!strcmp(value, "d1") || !strcmp(value, "fx"))
            settings.fxport = FXPORT_D1;
        else if(!strcmp(value, "d2") || !strcmp(value, "fxdevkit"))
            settings.fxport = FXPORT_D2;
        else if(!strcmp(value, "32") || !strcmp(value, "mini"))
            settings.fxport = FXPORT_E2;
        else
            settings.fxport = std::clamp<int>(nvalue, 0, FXPORT_NUM - 1);
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "display"))
    {
        if(!strcmp(value, "ssd1306"))
            settings.display = DISPLAY_SSD1306;
        else if(!strcmp(value, "ssd1309"))
            settings.display = DISPLAY_SSD1309;
        else if(!strcmp(value, "sh1106"))
            settings.display = DISPLAY_SH1106;
        else
            settings.display = std::clamp<int>(nvalue, 0, DISPLAY_NUM - 1);
        update_settings();
        r = 1;
    }
    else if(!strcmp(name, "loading"))
    {
        loading_indicator = bvalue;
        r = 1;
    }
    return r;
}

extern "C" void postsyncfs()
{
    fs_ready = true;
    load_savedata();
}

bool update_pixel_ratio()
{
    float ratio = platform_pixel_ratio();
    ratio *= (settings.uiscale * 0.25f + 0.5f);
    bool changed = (ratio != pixel_ratio);
    pixel_ratio = ratio;
    return changed;
}

void define_font()
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.RasterizerMultiply = 1.5f;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    io.Fonts->Clear();
#if !defined(ARDENS_NO_GUI) && !PROFILING
    io.Fonts->AddFontFromMemoryTTF(
        (void*)ProggyVector, sizeof(ProggyVector), 13.f * pixel_ratio, &cfg);
#endif
}

void rebuild_fonts()
{
    platform_destroy_fonts_texture();
    define_font();
    platform_create_fonts_texture();
}

void rescale_style()
{
    auto& style = ImGui::GetStyle();
    style = default_style;
    style.ScaleAllSizes(pixel_ratio);
}

void shutdown()
{
#ifndef ARDENS_NO_DEBUGGER
    ImPlot::DestroyContext();
#endif
}

void init()
{
    printf(
        "Ardens "
#ifdef ARDENS_PLAYER
        "Player "
#endif
        ARDENS_VERSION " by Peter Brown\n");

    ImGuiIO& io = ImGui::GetIO();

#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.mkdir('/offline');
    FS.mount(IDBFS, {}, '/offline');
    FS.syncfs(true, function(err) { ccall('postsyncfs', 'v'); });
    );
    io.IniFilename = "/offline/Ardens.ini";
    userpath = "/offline";
#else
    {
        static char ini_path[1024];
        get_user_config_folder(ini_path, sizeof(ini_path), "Ardens");
        userpath = ini_path;
        std::error_code ec;
        std::filesystem::create_directories(userpath, ec);
        strncpy(
            ini_path,
            (userpath / "Ardens.ini").generic_string().c_str(),
            sizeof(ini_path));
        io.IniFilename = ini_path;
    }
#endif

    printf("Data path: %s\n", userpath.generic_string().c_str());

    arduboy = std::make_unique<absim::arduboy_t>();
    arduboy->display.type = absim::display_t::SSD1306;

#ifndef ARDENS_NO_DEBUGGER
    ImPlot::CreateContext();
#endif

#ifndef ARDENS_NO_SAVED_SETTINGS
    init_settings();
#endif

#if !defined(__EMSCRIPTEN__) && !defined(ARDENS_NO_SAVED_SETTINGS)
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
    settings_loaded = true;
#endif

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

#if defined(__EMSCRIPTEN__) || defined(ARDENS_NO_SAVED_SETTINGS)
    io.IniFilename = nullptr;
#endif

    ImGui::StyleColorsDark();
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    style.Colors[ImGuiCol_PopupBg].w = 1.0f;

    default_style = style;

    arduboy->fx.erase_all_data();
    arduboy->cpu.adc_nondeterminism = settings.nondeterminism;
    arduboy->reset();
    arduboy->fx.min_page = 0xffff;
    arduboy->fx.max_page = 0xffff;

    ms_since_start = 0;
    ms_since_touch = MS_SHOW_TOUCH_CONTROLS;

#ifdef ARDENS_DIST
    load_file("", "game.arduboy", game_arduboy, game_arduboy_size);
#endif
}

void save_screenshot()
{
    char fname[256];
    time_t rawtime;
    struct tm* ti;
    time(&rawtime);
    ti = localtime(&rawtime);
    (void)snprintf(fname, sizeof(fname),
        "screenshot_%04d%02d%02d%02d%02d%02d.png",
        ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
        ti->tm_hour + 1, ti->tm_min, ti->tm_sec);
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
    file_download("screenshot.png", fname, "image/x-png");
#else
    stbi_write_png(fname, w, h, 4, recording_pixels(true), stride);
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
    char fname[256];
    time_t rawtime;
    struct tm* ti;
    time(&rawtime);
    ti = localtime(&rawtime);
    (void)snprintf(fname, sizeof(fname),
        "ardens_%04d%02d%02d%02d%02d%02d.snapshot",
        ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
        ti->tm_hour + 1, ti->tm_min, ti->tm_sec);
#ifdef __EMSCRIPTEN__
    std::ofstream f("ardens.snapshot", std::ios::binary);
    if(arduboy->save_snapshot(f))
    {
        f.close();
        file_download("ardens.snapshot", fname, "application/octet-stream");
    }
#else
    std::ofstream f(fname, std::ios::binary);
    arduboy->save_snapshot(f);
#endif
#endif
}

std::string preferred_title()
{
#ifdef ARDENS_DIST
    if(arduboy && !arduboy->title.empty())
        return arduboy->title;
#endif
    return ARDENS_TITLE;
}

void frame_logic()
{
    ImGuiIO& io = ImGui::GetIO();

    arduboy->cpu.adc_nondeterminism = settings.nondeterminism;
    if(!touch_points.empty())
        ms_since_touch = 0;

#ifndef ARDENS_DIST
    file_watch_check();
#endif

#ifdef __EMSCRIPTEN__
    if(done) emscripten_cancel_main_loop();
#endif

    // advance simulation
    uint64_t dt = platform_get_ms_dt();
    ms_since_start += dt;
    ms_since_touch += dt;
    if(dt > 100) dt = 100;
    if(!arduboy->paused)
    {
        // PINF: 4,5,6,7=D,L,R,U
        // PINE: 6=A
        // PINB: 4=B
        uint8_t pinf = 0xf0;
        uint8_t pine = 0x40;
        uint8_t pinb = 0x10;

        if(!ImGui::GetIO().WantCaptureKeyboard)
        {
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

            if(ImGui::IsKeyDown(keys[0]) || touch.btns[tkeys[0]]) pinf &= ~0x80;
            if(ImGui::IsKeyDown(keys[1]) || touch.btns[tkeys[1]]) pinf &= ~0x40;
            if(ImGui::IsKeyDown(keys[2]) || touch.btns[tkeys[2]]) pinf &= ~0x10;
            if(ImGui::IsKeyDown(keys[3]) || touch.btns[tkeys[3]]) pinf &= ~0x20;

            if( ImGui::IsKeyDown(ImGuiKey_A) ||
                ImGui::IsKeyDown(ImGuiKey_Z) ||
                touch.btns[TOUCH_A])
                pine &= ~0x40;
            if( ImGui::IsKeyDown(ImGuiKey_B) ||
                ImGui::IsKeyDown(ImGuiKey_S) ||
                ImGui::IsKeyDown(ImGuiKey_X) ||
                touch.btns[TOUCH_B])
                pinb &= ~0x10;

            arduboy->cpu.data[0x23] = pinb;
            arduboy->cpu.data[0x2c] = pine;
            arduboy->cpu.data[0x2f] = pinf;
        }

#if PROFILING
        dt = 200;
#endif

        bool prev_paused = arduboy->paused;
        arduboy->frame_bytes_total = 1024;

        arduboy->cpu.enabled_autobreaks.reset();
#ifndef ARDENS_NO_GUI
        arduboy->cpu.enabled_autobreaks.set(absim::AB_BREAK);
        for(int i = 1; i < absim::AB_NUM; ++i)
            if(settings.ab.index(i))
                arduboy->cpu.enabled_autobreaks.set(i);
#endif

        arduboy->allow_nonstep_breakpoints =
            arduboy->break_step == 0xffffffff || settings.enable_step_breaks;
        arduboy->display.enable_filter = settings.display_auto_filter;

        arduboy->display.enable_current_limiting = (settings.display_current_modeling != 0);
        arduboy->display.ref_segment_current = 0.195f;
        switch(settings.display_current_modeling)
        {
        case 0:  arduboy->display.current_limit_slope = 0.f;   break;
        case 1:  arduboy->display.current_limit_slope = 0.75f; break;
        case 2:  arduboy->display.current_limit_slope = 0.45f; break;
        case 3:  arduboy->display.current_limit_slope = 0.f;   break;
        default: arduboy->display.current_limit_slope = 0.f;   break;
        }

        switch(settings.fxport)
        {
        case FXPORT_D1:
            arduboy->fxport_reg = 0x2b;
            arduboy->fxport_mask = 1 << 1;
            break;
        case FXPORT_D2:
            arduboy->fxport_reg = 0x2b;
            arduboy->fxport_mask = 1 << 2;
            break;
        case FXPORT_E2:
            arduboy->fxport_reg = 0x2e;
            arduboy->fxport_mask = 1 << 2;
            break;
        default:
            break;
        }

        switch(settings.display)
        {
        case DISPLAY_SSD1306:
            arduboy->display.type = absim::display_t::SSD1306;
            break;
        case DISPLAY_SSD1309:
            arduboy->display.type = absim::display_t::SSD1309;
            break;
        case DISPLAY_SH1106:
            arduboy->display.type = absim::display_t::SH1106;
            break;
        default:
            break;
        }

        constexpr uint64_t MS_TO_PS = 1000000000ull;
        uint64_t dtps = dt * MS_TO_PS * 1000 / simulation_slowdown;
        if(gif_recording)
        {
            constexpr uint64_t DT_20_MS = 20 * MS_TO_PS;
            uint64_t ps = DT_20_MS - gif_ps_rem;
            while(dtps >= ps)
            {
                arduboy->advance(ps);
                send_gif_frame(2, recording_pixels(false));
                dtps -= ps;
                ps = DT_20_MS;
                gif_ps_rem = 0;
            }
            gif_ps_rem += dtps;
        }
        if(dtps > 0)
            arduboy->advance(dtps * SPEEDUP);

        check_save_savedata();

        if(arduboy->paused && !prev_paused)
            disassembly_scroll_addr = arduboy->cpu.pc * 2;
        //if(!settings.enable_stack_breaks)
        //    arduboy->cpu.stack_overflow = false;

        // consume sound buffer
        send_wav_audio();
        process_sound_samples();
#if !PROFILING
        if(!arduboy->cpu.sound_buffer.empty() && simulation_slowdown == 1000)
            platform_send_sound();
#endif
        arduboy->cpu.sound_buffer.clear();
    }
    else
    {
        arduboy->break_step = 0xffffffff;
    }

    // update framebuffer texture
    if(arduboy->cpu.decoded)
    {
        recreate_display_texture();
        int z = display_texture_zoom;
        std::vector<uint8_t> pixels;
        pixels.resize(128 * 64 * 4 * z * z);
        scalenx(pixels.data(), arduboy->display.filtered_pixels.data(), true);
        platform_update_texture(display_texture, pixels.data(), pixels.size());

#if ALLOW_SCREENSHOTS
        if(arduboy->cpu.decoded && ImGui::IsKeyPressed(ImGuiKey_F4, false))
            take_snapshot();
        if(arduboy->cpu.decoded && ImGui::IsKeyPressed(ImGuiKey_F2, false))
            save_screenshot();
        if(arduboy->cpu.decoded && ImGui::IsKeyPressed(ImGuiKey_F3, false))
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
        arduboy->paused = !arduboy->paused;
#endif
    if(ImGui::IsKeyPressed(ImGuiKey_F8, false))
        arduboy->reset();

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
    {
        rescale_style();
        rebuild_fonts();
    }
}

void imgui_content()
{
    ImGuiIO& io = ImGui::GetIO();

    if(!arduboy->cpu.decoded)
    {
        auto* d = ImGui::GetBackgroundDrawList();
        auto size = ImGui::GetMainViewport()->Size;
        float w = 100.f * pixel_ratio;
        float pad = 25.f * pixel_ratio;
        w = std::min(w, size.x - pad);
        w = std::min(w, size.y - pad);
        w = std::max(w, pad);
        float w2 = w * 0.5f;
        float cx = size.x * 0.5f;
        float cy = size.y * 0.5f;
        constexpr uint8_t shade = 200;
        auto color = IM_COL32(shade, shade, shade, 255);
        float thickness = w * 0.04f;
        if(loading_indicator)
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
        else
        {
            d->AddRect(
                { cx - w2, cy - w2 },
                { cx + w2, cy + w2 },
                color,
                10.f * pixel_ratio,
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
    if(!dropfile_err.empty() && !ImGui::IsPopupOpen("File Load Error"))
        ImGui::OpenPopup("File Load Error");

    ImGui::SetNextWindowSize({ 500 * pixel_ratio, 0 });
    if(ImGui::BeginPopupModal("File Load Error", NULL,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(dropfile_err.c_str());
        ImGui::PopTextWrapPos();
        if(ImGui::Button("OK", ImVec2(120 * pixel_ratio, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Enter) ||
            dropfile_err.empty())
        {
            dropfile_err.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
#endif

#ifndef ARDENS_NO_GUI
    if(arduboy->cpu.should_autobreak_gui())
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

    ImGui::SetNextWindowSize({ 300 * pixel_ratio, 0 });
    if(ImGui::BeginPopupModal("Auto-Break", NULL,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings))
    {
        size_t ab = 0;
        auto mask = arduboy->cpu.autobreaks & arduboy->cpu.enabled_autobreaks;
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
        if(ImGui::Button("OK", ImVec2(120 * pixel_ratio, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            arduboy->cpu.autobreaks = 0;
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
