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

#define PROFILING 0

#if ALLOW_SCREENSHOTS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#include <cmath>
#include <algorithm>
#include <fstream>
#include <strstream>

#include "common.hpp"

constexpr uint32_t AUDIO_FREQ = 16000000 / absim::atmega32u4_t::SOUND_CYCLES;
#ifdef __EMSCRIPTEN__
constexpr uint32_t MAX_AUDIO_LATENCY_SAMPLES = 4096;
#else
constexpr uint32_t MAX_AUDIO_LATENCY_SAMPLES = 2048;
#endif

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

extern unsigned char const ProggyVector[198188];

static SDL_AudioDeviceID audio_device;
static SDL_AudioSpec audio_spec;

SDL_Texture* display_texture = nullptr;
SDL_Texture* display_buffer_texture = nullptr;
std::unique_ptr<absim::arduboy_t> arduboy;

int profiler_selected_hotspot = -1;
int disassembly_scroll_addr = -1;
bool scroll_addr_to_top = false;
int simulation_slowdown = 1000;
int fx_data_scroll_addr = -1;

int display_buffer_addr = -1;
int display_buffer_w = 128;
int display_buffer_h = 64;

static uint64_t pt;
static std::string dropfile_err;
static bool done = false;
bool layout_done = false;
bool settings_loaded = false;
#ifdef __EMSCRIPTEN__
bool fs_ready = false;
#else
bool fs_ready = true;
#endif

SDL_Window* window;
SDL_Renderer* renderer;

float pixel_ratio = 1.f;
static ImGuiStyle default_style;

extern "C" int setparam(char const* name, char const* value)
{
    if(!name || !value) return 0;
    std::string p(name);
    if(p == "z")
    {
        settings.fullzoom = (*value == '1');
        update_settings();
        return 1;
    }
    else if(p == "g")
    {
        int g = 1;
        if(*value == '2') g = 2;
        if(*value == '3') g = 3;
        settings.num_pixel_history = g;
        update_settings();
        return 1;
    }
    return 0;
}

extern "C" void postsyncfs()
{
    fs_ready = true;
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

static void define_font()
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.RasterizerMultiply = 1.5f;
    io.Fonts->Clear();
    io.Fonts->AddFontFromMemoryTTF(
        (void*)ProggyVector, sizeof(ProggyVector), 13.f * pixel_ratio, &cfg);
#ifdef __EMSCRIPTEN__
    //io.FontGlobalScale = 1.f / pixel_ratio;
#endif
}

static bool update_pixel_ratio()
{
    float ratio = 1.f;
#ifdef _WIN32
    SDL_GetDisplayDPI(0, nullptr, &ratio, nullptr);
    ratio *= 1.f / 96;
#endif
#ifdef __EMSCRIPTEN__
    ratio = (float)emscripten_get_device_pixel_ratio();
#endif
    bool changed = (ratio != pixel_ratio);
    pixel_ratio = ratio;
    return changed;
}

static void rebuild_fonts()
{
    ImGui_ImplSDLRenderer_DestroyFontsTexture();
    define_font();
    ImGui_ImplSDLRenderer_CreateFontsTexture();
}

static void rescale_style()
{
    auto& style = ImGui::GetStyle();
    style = default_style;
    style.ScaleAllSizes(pixel_ratio);
}

extern "C" int load_file(char const* filename, uint8_t const* data, size_t size)
{
    std::istrstream f((char const*)data, size);
    dropfile_err = arduboy->load_file(filename, f);
    return 0;
}

static void main_loop()
{
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImGuiIO& io = ImGui::GetIO();

#ifdef __EMSCRIPTEN__
    if(done) emscripten_cancel_main_loop();
#endif

    // advance simulation
    uint64_t t = SDL_GetTicks64();
    uint64_t dt = t - pt;
    if(dt > 30) dt = 30;
    pt = t;
    arduboy->cpu.stack_overflow = false;
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
            if(ImGui::IsKeyDown(ImGuiKey_DownArrow)) pinf &= ~0x10;
            if(ImGui::IsKeyDown(ImGuiKey_LeftArrow)) pinf &= ~0x20;
            if(ImGui::IsKeyDown(ImGuiKey_RightArrow)) pinf &= ~0x40;
            if(ImGui::IsKeyDown(ImGuiKey_UpArrow)) pinf &= ~0x80;
            if(ImGui::IsKeyDown(ImGuiKey_A)) pine &= ~0x40;
            if(ImGui::IsKeyDown(ImGuiKey_B) || ImGui::IsKeyDown(ImGuiKey_S)) pinb &= ~0x10;

            arduboy->cpu.data[0x23] = pinb;
            arduboy->cpu.data[0x2c] = pine;
            arduboy->cpu.data[0x2f] = pinf;
        }

#if PROFILING
        dt = 200;
#endif

        bool prev_paused = arduboy->paused;
        arduboy->frame_bytes_total = (settings.num_pixel_history == 1 ? 1024 : 0);
        arduboy->cpu.enable_stack_break = settings.enable_stack_breaks;
        arduboy->allow_nonstep_breakpoints =
            arduboy->break_step == 0xffffffff || settings.enable_step_breaks;
        arduboy->advance(dt * 1000000000000ull / simulation_slowdown);
        if(arduboy->paused && !prev_paused)
            disassembly_scroll_addr = arduboy->cpu.pc * 2;
        if(!settings.enable_stack_breaks)
            arduboy->cpu.stack_overflow = false;

        // consume sound buffer
#if !PROFILING
        if(!arduboy->cpu.sound_buffer.empty() && simulation_slowdown == 1000)
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
#endif
        arduboy->cpu.sound_buffer.clear();
        SDL_PauseAudioDevice(audio_device, 0);
    }
    else
    {
        arduboy->break_step = 0xffffffff;
        SDL_PauseAudioDevice(audio_device, 1);
    }

    // update framebuffer texture
    if(arduboy->cpu.decoded)
    {
        void* pixels = nullptr;
        int pitch = 0;
        arduboy->display.num_pixel_history = settings.num_pixel_history;
        arduboy->display.filter_pixels();

        recreate_display_texture();
        SDL_LockTexture(display_texture, nullptr, &pixels, &pitch);
        scalenx((uint8_t*)pixels, arduboy->display.filtered_pixels.data(), true);
        SDL_UnlockTexture(display_texture);

#if ALLOW_SCREENSHOTS
        if(arduboy->cpu.decoded && ImGui::IsKeyPressed(ImGuiKey_F4, false))
        {
            char fname[256];
            time_t rawtime;
            struct tm* ti;
            time(&rawtime);
            ti = localtime(&rawtime);
            (void)snprintf(fname, sizeof(fname),
                "absim_%04d%02d%02d%02d%02d%02d.snapshot",
                ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
                ti->tm_hour + 1, ti->tm_min, ti->tm_sec);
#ifdef __EMSCRIPTEN__
            std::ofstream f("absim.snapshot", std::ios::binary);
            if(arduboy->save_snapshot(f))
            {
                f.close();
                file_download("absim.snapshot", fname, "application/octet-stream");
            }
#else
            std::ofstream f(fname, std::ios::binary);
            arduboy->save_snapshot(f);
#endif
        }
        if(arduboy->cpu.decoded && ImGui::IsKeyPressed(ImGuiKey_F2, false))
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
#ifdef __EMSCRIPTEN__
            stbi_write_png("screenshot.png", 128 * z, 64 * z, 4, recording_pixels(true), 128 * 4 * z);
            file_download("screenshot.png", fname, "image/x-png");
#else
            stbi_write_png(fname, 128 * z, 64 * z, 4, recording_pixels(true), 128 * 4 * z);
#endif
        }
        if(gif_recording && arduboy->paused)
        {
            screen_recording_toggle(recording_pixels(false));
        }
        else if(simulation_slowdown == 1000 && ImGui::IsKeyPressed(ImGuiKey_F3, false))
        {
            screen_recording_toggle(recording_pixels(false));
        }
        else if(gif_recording)
        {
            gif_ms_rem += dt;
            while(gif_ms_rem >= 20)
            {
                send_gif_frame(2, recording_pixels(false));
                gif_ms_rem -= 20;
            }
        }
#endif
    }

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
        }
    }

    if(update_pixel_ratio())
    {
        rescale_style();
        rebuild_fonts();
    }

    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();

    if(!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_P, false))
    {
        settings.fullzoom = !settings.fullzoom;
        update_settings();
    }

    ImGui::NewFrame();

    if(!arduboy->cpu.decoded)
    {
        auto* d = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
        static char const* const MSG = "Drag a .hex or .arduboy file onto this window";
        auto t = ImGui::CalcTextSize(MSG);
        auto size = ImGui::GetMainViewport()->Size;
        d->AddText(
            ImVec2((size.x - t.x) * 0.5f, (size.y - t.y) * 0.5f),
            ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]),
            MSG);
    }

    if(settings.fullzoom)
        view_player();
    else
        view_debugger();

    settings_modal();

    if(!dropfile_err.empty() && !ImGui::IsPopupOpen("File Load Error"))
        ImGui::OpenPopup("File Load Error");

    ImGui::SetNextWindowSize({ 500, 0 });
    if(ImGui::BeginPopupModal("File Load Error", NULL,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(dropfile_err.c_str());
        ImGui::PopTextWrapPos();
        if(ImGui::Button("OK", ImVec2(120, 0)))
        {
            dropfile_err.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if(arduboy->cpu.stack_overflow)
        ImGui::OpenPopup("Stack Overflow");

    ImGui::SetNextWindowSize({ 300, 0 });
    if(ImGui::BeginPopupModal("Stack Overflow", NULL,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted("Auto-break due to stack overflow!");
        ImGui::PopTextWrapPos();
        if(ImGui::Button("OK", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

#if PROFILING
    ImGui::ShowMetricsWindow();
#endif
    //ImGui::ShowDemoWindow();

    ImGui::Render();

    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

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
#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.mkdir('/offline');
        FS.mount(IDBFS, {}, '/offline');
        FS.syncfs(true, function(err) { ccall('postsyncfs', 'v'); });
    );
#endif

    arduboy = std::make_unique<absim::arduboy_t>();

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
    }

#if PROFILING
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    init_settings();
    ImGuiIO& io = ImGui::GetIO();
#ifndef __EMSCRIPTEN__
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
    settings_loaded = true;
#endif

    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

#ifdef __EMSCRIPTEN__
    io.IniFilename = nullptr;
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

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    default_style = style;

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

    arduboy->fx.erase_all_data();
    arduboy->reset();
    arduboy->fx.min_page = 0xffff;
    arduboy->fx.max_page = 0xffff;

    pt = SDL_GetTicks64();
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

    if(display_texture)
    {
        SDL_DestroyTexture(display_texture);
        display_texture = nullptr;
    }
    if(display_buffer_texture)
    {
        SDL_DestroyTexture(display_buffer_texture);
        display_buffer_texture = nullptr;
    }

    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_CloseAudioDevice(audio_device);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
