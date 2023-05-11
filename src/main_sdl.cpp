#ifdef ABSIM_PLATFORM_SDL

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "implot.h"
#include <stdio.h>
#include <time.h>
#include <SDL.h>

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

#include "common.hpp"

#ifdef __EMSCRIPTEN__
constexpr uint32_t MAX_AUDIO_LATENCY_SAMPLES = 4096;
#else
constexpr uint32_t MAX_AUDIO_LATENCY_SAMPLES = 2048;
#endif

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

constexpr ImVec4 clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };

static SDL_AudioDeviceID audio_device;
static SDL_AudioSpec audio_spec;

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
    SDL_SetTextureScaleMode((SDL_Texture*)t, SDL_ScaleModeLinear);
}

void platform_texture_scale_nearest(texture_t t)
{
    SDL_SetTextureScaleMode((SDL_Texture*)t, SDL_ScaleModeNearest);
}

void platform_set_clipboard_text(char const* str)
{
    SDL_SetClipboardText(str);
}

uint64_t platform_get_ms_dt()
{
    static uint64_t pt = 0;
    uint64_t t = SDL_GetTicks64();
    if(t <= pt) return 0;
    uint64_t dt = t - pt;
    pt = t;
    return dt;
}

void platform_send_sound()
{
    std::vector<int16_t> buf;
    buf.swap(arduboy->cpu.sound_buffer);
    constexpr size_t SAMPLE_SIZE = sizeof(buf[0]);
    size_t num_bytes = buf.size() * SAMPLE_SIZE;
    uint32_t queued_bytes = SDL_GetQueuedAudioSize(audio_device);
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
        SDL_QueueAudio(
            audio_device,
            buf.data(),
            buf.size() * sizeof(buf[0]));
    }
}

float platform_pixel_ratio()
{
    float ratio = 1.f;
#ifdef _WIN32
    SDL_GetDisplayDPI(0, nullptr, &ratio, nullptr);
    ratio *= 1.f / 96;
#endif
#ifdef __EMSCRIPTEN__
    ratio = (float)emscripten_get_device_pixel_ratio();
#endif
    return ratio;
}

void platform_destroy_fonts_texture()
{
    ImGui_ImplSDLRenderer_DestroyFontsTexture();
}

void platform_create_fonts_texture()
{
    ImGui_ImplSDLRenderer_CreateFontsTexture();
}

void platform_open_url(char const* url)
{
    SDL_OpenURL(url);
}

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

static void main_loop()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if(event.type == SDL_QUIT)
            done = true;
        if(event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(window))
        {
            if(event.window.event == SDL_WINDOWEVENT_CLOSE)
                done = true;
        }
        if(event.type == SDL_DROPFILE)
        {
            std::ifstream f(event.drop.file, std::ios::binary);
            dropfile_err = arduboy->load_file(event.drop.file, f);
            SDL_free(event.drop.file);
            if(dropfile_err.empty()) load_savedata();
        }
    }

    frame_logic();

    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();

    ImGui::NewFrame();

    imgui_content();

    ImGui::Render();

    SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

#if ALLOW_SCREENSHOTS
    if(ImGui::IsKeyPressed(ImGuiKey_F1, false))
    {
        int w = 0, h = 0;
        Uint32 format = SDL_PIXELFORMAT_RGB24;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        SDL_Surface* ss = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, format);
        SDL_RenderReadPixels(renderer, NULL, format, ss->pixels, ss->pitch);
        char fname[256];
        time_t rawtime;
        struct tm* ti;
        time(&rawtime);
        ti = localtime(&rawtime);
        (void)snprintf(fname, sizeof(fname),
            "screenshot_%04d%02d%02d%02d%02d%02d.png",
            ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
            ti->tm_hour + 1, ti->tm_min, ti->tm_sec);
#ifdef __EMSCRIPTEN__
        stbi_write_png("screenshot.png", w, h, 3, ss->pixels, ss->pitch);
        file_download("screenshot.png", fname, "image/x-png");
#else
        stbi_write_png(fname, w, h, 3, ss->pixels, ss->pitch);
#endif
        SDL_FreeSurface(ss);
    }
#endif

    SDL_RenderPresent(renderer);
}

int main(int argc, char** argv)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    init();

#ifdef _WIN32
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    {
        SDL_AudioSpec desired;
        memset(&desired, 0, sizeof(desired));
        desired.freq = AUDIO_FREQ;
        desired.format = AUDIO_S16SYS;
        desired.channels = 1;
        desired.samples = MAX_AUDIO_LATENCY_SAMPLES;
        audio_device = SDL_OpenAudioDevice(
            nullptr, 0,
            &desired,
            &audio_spec,
            0);
        SDL_PauseAudioDevice(audio_device, 0);
    }

#if PROFILING
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
#endif

    // Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(
        SDL_WINDOW_RESIZABLE |
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
        SDL_WINDOW_ALLOW_HIGHDPI |
#endif
        0);
    window = SDL_CreateWindow("arduboy_sim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
    {
        SDL_Log("Error creating SDL_Renderer!");
        return 0;
    }

    update_pixel_ratio();
    rescale_style();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);

    define_font();

    recreate_display_texture();
    display_buffer_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        128,
        64);

    done = false;
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, -1, 1);
#else
    for(int arg = 1; arg < argc; ++arg)
    {
        SDL_Event file_drop_event;
        file_drop_event.type = SDL_DROPFILE;
        file_drop_event.drop.timestamp = SDL_GetTicks();
        file_drop_event.drop.file = SDL_strdup(argv[arg]);
        file_drop_event.drop.windowID = SDL_GetWindowID(window);
        SDL_PushEvent(&file_drop_event);
    }
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

    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_CloseAudioDevice(audio_device);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

#endif
