#ifdef ARDENS_PLATFORM_SDL

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <implot/implot.h>
#include <stdio.h>
#include <time.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef _WIN32
#include <ShellScalingApi.h>
#pragma comment(lib, "shcore.lib")
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
#include "stb_image_write.h"
#endif

#define PROFILING 0

#include <cmath>
#include <algorithm>
#include <fstream>

#define SOKOL_IMPL
#ifndef __EMSCRIPTEN__
#include "sokol/sokol_args.h"
#endif

#include "common.hpp"

#ifdef __EMSCRIPTEN__
constexpr uint32_t MAX_AUDIO_LATENCY_SAMPLES = 4096;
#else
constexpr uint32_t MAX_AUDIO_LATENCY_SAMPLES = 2048;
#endif

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

constexpr ImVec4 clear_color = { CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };

static SDL_AudioStream* audio_stream;

static SDL_Window* window;
static SDL_Renderer* renderer;

void platform_destroy_texture(texture_t t)
{
    if(t != nullptr)
        SDL_DestroyTexture((SDL_Texture*)t);
}

texture_t platform_create_texture(int w, int h)
{
    return SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        w,
        h);
}

void platform_update_texture(texture_t t, void const* data, size_t n)
{
    void* pixels;
    int pitch;
    SDL_LockTexture((SDL_Texture*)t, nullptr, &pixels, &pitch);
    memcpy(pixels, data, n);
    SDL_UnlockTexture((SDL_Texture*)t);
}

void platform_texture_scale_linear(texture_t t)
{
    SDL_SetTextureScaleMode((SDL_Texture*)t, SDL_SCALEMODE_LINEAR);
}

void platform_texture_scale_nearest(texture_t t)
{
    SDL_SetTextureScaleMode((SDL_Texture*)t, SDL_SCALEMODE_NEAREST);
}

void platform_set_clipboard_text(char const* str)
{
    SDL_SetClipboardText(str);
}

uint64_t platform_get_ms_dt()
{
    static uint64_t pt = 0;
    uint64_t t = SDL_GetTicks();
    if(t <= pt) return 0;
    uint64_t dt = t - pt;
    pt = t;
    return dt;
}

void platform_send_sound()
{
    std::vector<int16_t> buf;
    buf.swap(arduboy.cpu.sound_buffer);
    constexpr size_t SAMPLE_SIZE = sizeof(buf[0]);
    size_t num_bytes = buf.size() * SAMPLE_SIZE;
    uint32_t queued_bytes = SDL_GetAudioStreamQueued(audio_stream);
    constexpr uint32_t BUFFER_BYTES = MAX_AUDIO_LATENCY_SAMPLES * SAMPLE_SIZE * 2;
    if(queued_bytes > BUFFER_BYTES)
    {
        buf.clear();
    }
    else if(num_bytes + queued_bytes > BUFFER_BYTES)
    {
        num_bytes = BUFFER_BYTES - queued_bytes;
        buf.resize(num_bytes / SAMPLE_SIZE);
    }
    if(!buf.empty())
    {
        float gain = volume_gain() * 32768;
        for(auto& b : buf)
            b = int16_t(std::clamp<float>(gain * b, INT16_MIN, INT16_MAX));
        SDL_PutAudioStreamData(
            audio_stream,
            buf.data(),
            int(buf.size() * sizeof(buf[0])));
    }
}

float platform_pixel_ratio()
{
    return SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window));
}

void platform_destroy_fonts_texture()
{
    ImGui_ImplSDLRenderer3_DestroyFontsTexture();
}

void platform_create_fonts_texture()
{
    ImGui_ImplSDLRenderer3_CreateFontsTexture();
}

void platform_open_url(char const* url)
{
    SDL_OpenURL(url);
}

bool platform_toggle_fullscreen()
{
    settings.window_fullscreen = !settings.window_fullscreen;
    SDL_SetWindowFullscreen(window, (SDL_bool)settings.window_fullscreen);
    update_settings();
    return settings.window_fullscreen;
}

static void main_loop()
{
    auto& io = ImGui::GetIO();
    
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if(event.type == SDL_EVENT_QUIT)
            done = true;
        if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
            done = true;
        if(event.type == SDL_EVENT_DROP_FILE)
        {
#if !defined(__EMSCRIPTEN__) && !defined(ARDENS_DIST) && !defined(ARDENS_FLASHCART)
            std::ifstream f(event.drop.data, std::ios::binary);
            std::vector<uint8_t> fdata(
                (std::istreambuf_iterator<char>(f)),
                std::istreambuf_iterator<char>());
            load_file("", event.drop.data, fdata.data(), fdata.size());
#endif
            //SDL_free(event.drop.file);
        }
        if(event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION)
        {
            int w = 0, h = 0;
            SDL_GetWindowSize(window, &w, &h);
            first_touch = true;
            auto& tp = touch_points[event.tfinger.fingerID];
            tp = { event.tfinger.x * w, event.tfinger.y * h };
        }
        if(event.type == SDL_EVENT_FINGER_UP)
        {
            touch_points.erase(event.tfinger.fingerID);
        }
        if(event.type == SDL_EVENT_WINDOW_RESIZED &&
            !settings.window_maximized &&
            !settings.window_fullscreen)
        {
            settings.window_w = event.window.data1;
            settings.window_h = event.window.data2;
            update_settings();
        }
        if(event.type == SDL_EVENT_WINDOW_MAXIMIZED)
        {
            settings.window_maximized = true;
            update_settings();
        }
        if(event.type == SDL_EVENT_WINDOW_RESTORED)
        {
            settings.window_maximized = false;
            update_settings();
        }
    }

    frame_logic();

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    do
    {
        int count = 0;
        auto* j = SDL_GetGamepads(&count);
        if(!j) break;
        auto& io = ImGui::GetIO();
        for(int i = 0; i < count; ++i)
        {
            if(!SDL_IsGamepad(j[i])) continue;
            auto* g = SDL_OpenGamepad(j[i]);
            if(!g) continue;
            io.AddKeyEvent(ImGuiKey_A         , SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_SOUTH     ) != 0);
            io.AddKeyEvent(ImGuiKey_B         , SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_EAST      ) != 0);
            io.AddKeyEvent(ImGuiKey_UpArrow   , SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_UP   ) != 0);
            io.AddKeyEvent(ImGuiKey_DownArrow , SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_DOWN ) != 0);
            io.AddKeyEvent(ImGuiKey_LeftArrow , SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_LEFT ) != 0);
            io.AddKeyEvent(ImGuiKey_RightArrow, SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) != 0);
            break;
        }
    } while(0);

    ImGui::NewFrame();

    imgui_content();

    ImGui::Render();

#ifdef __APPLE__
    // why?
    SDL_SetRenderScale(
        renderer,
        io.DisplayFramebufferScale.x,
        io.DisplayFramebufferScale.y);
#endif
    SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

#if ALLOW_SCREENSHOTS && !defined(ARDENS_PLAYER)
    if(ImGui::IsKeyPressed(ImGuiKey_F1, false))
    {
        int w = 0, h = 0;
        SDL_PixelFormat format = SDL_PIXELFORMAT_RGB24;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        SDL_Surface* rs = SDL_RenderReadPixels(renderer, NULL);
        SDL_Surface* ss = SDL_ConvertSurface(rs, format);
        if(ss)
        {
            char fname[256];
            time_t rawtime;
            struct tm* ti;
            time(&rawtime);
            ti = localtime(&rawtime);
            (void)snprintf(fname, sizeof(fname),
                "window_%04d%02d%02d%02d%02d%02d.png",
                ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
                ti->tm_hour + 1, ti->tm_min, ti->tm_sec);
#ifdef __EMSCRIPTEN__
            stbi_write_png("screenshot.png", w, h, 3, ss->pixels, ss->pitch);
            file_download("screenshot.png", fname, "image/x-png");
#else
            stbi_write_png(fname, w, h, 3, ss->pixels, ss->pitch);
#endif
        }
        SDL_DestroySurface(rs);
        SDL_DestroySurface(ss);
    }
#endif

    SDL_RenderPresent(renderer);
}

void platform_quit()
{
    SDL_Event e;
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

void platform_set_title(char const* title)
{
    (void)SDL_SetWindowTitle(window, title);
}

void platform_set_window_size(int w, int h)
{
    (void)SDL_SetWindowSize(window, w, h);
}

void platform_set_window_maximized(bool maximized)
{
    if(maximized)
        (void)SDL_MaximizeWindow(window);
    else
        (void)SDL_RestoreWindow(window);
}

int main(int argc, char** argv)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    init();

    int width = settings.window_w;
    int height = settings.window_h;

#ifndef __EMSCRIPTEN__
    {
        sargs_desc d{};
        d.argc = argc;
        d.argv = argv;
        sargs_setup(&d);

        for(int i = 0; i < sargs_num_args(); ++i)
        {
            char const* k = sargs_key_at(i);
            char const* v = sargs_value_at(i);
            if(0 != strcmp(k, "size")) continue;
            int w = width, h = height;
            if(2 == sscanf(v, "%dx%d", &w, &h))
                width = w, height = h;
            break;
        }
    }
#endif

    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMEPAD) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    {
        static SDL_AudioSpec const spec = {
            SDL_AUDIO_S16, 1, AUDIO_FREQ
        };
        audio_stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audio_stream));
    }

#if PROFILING
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
#endif

    // Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(
        SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_HIGH_PIXEL_DENSITY |
        0);
    if(settings.window_fullscreen)
        window_flags |= SDL_WINDOW_FULLSCREEN;
    else if(settings.window_maximized)
        window_flags |= SDL_WINDOW_MAXIMIZED;
    window = SDL_CreateWindow(preferred_title().c_str(), width, height, window_flags);

    renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == NULL)
    {
        SDL_Log("Error creating SDL_Renderer!");
        return 0;
    }

    update_pixel_ratio();
    rescale_style();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    define_font();

    recreate_display_texture();
    display_buffer_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        128,
        64);

    done = false;

#if !defined(__EMSCRIPTEN__)
    for(int i = 0; i < sargs_num_args(); ++i)
    {
        char const* value = sargs_value_at(i);
        if(!setparam(sargs_key_at(i), value))
        {
#if !defined(ARDENS_DIST)
            std::ifstream f(value, std::ios::in | std::ios::binary);
            if(f)
            {
                bool save = !strcmp(sargs_key_at(i), "save");
                dropfile_err = arduboy.load_file(value, f, save);
                autoset_from_device_type();
                if(dropfile_err.empty())
                {
                    load_savedata();
                    if(!save) file_watch(value);
                }
            }
            else
                dropfile_err = std::string("Could not open file: \"") + value + "\"";
#endif
    }
}
#endif
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, -1, 1);
#else
    while(!done)
        main_loop();
#endif

    shutdown();

    if(display_texture)
    {
        SDL_DestroyTexture((SDL_Texture*)display_texture);
        display_texture = nullptr;
    }
    if(display_buffer_texture)
    {
        SDL_DestroyTexture((SDL_Texture*)display_buffer_texture);
        display_buffer_texture = nullptr;
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyAudioStream(audio_stream);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

#endif
