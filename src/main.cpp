#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
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

#include <cmath>
#include <algorithm>
#include <fstream>
#include <strstream>

#include "absim.hpp"
#include "absim_instructions.hpp"

#define PROFILING 0

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

extern unsigned char const ProggyVector[198188];

static SDL_Texture* framebuffer_texture;
absim::arduboy_t arduboy;

int profiler_selected_hotspot = -1;
int disassembly_scroll_addr = -1;
bool profiler_cycle_counts = false;

int simulation_slowdown = 1;

void window_disassembly(bool& open);
void window_profiler(bool& open);
void window_display(bool& open, void* tex);
void window_display_internals(bool& open);
void window_data_space(bool& open);
void window_simulation(bool& open);
void window_call_stack(bool& open);

static bool open_disassembly = true;
static bool open_display = true;
static bool open_display_internals = true;
static bool open_data_space = true;
static bool open_profiler = true;
static bool open_simulation = true;
static bool open_call_stack = true;

static uint64_t pt;
static std::string dropfile_err;
static bool done = false;

static SDL_Window* window;
static SDL_Renderer* renderer;

static float pixel_ratio = 1.f;
static ImGuiStyle default_style;

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
    dropfile_err = arduboy.load_file(filename, f);
    dropfile_err = "Hello!";
    return 0;
}

static void main_loop()
{
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImGuiIO& io = ImGui::GetIO();

#ifdef __EMSCRIPTEN__
    if(done) emscripten_cancel_main_loop();
#endif

#if ALLOW_SCREENSHOTS
    if(ImGui::IsKeyPressed(ImGuiKey_F12, false))
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
        stbi_write_png(fname, w, h, 3, ss->pixels, ss->pitch);
        SDL_FreeSurface(ss);
    }
#endif

    // advance simulation
    uint64_t t = SDL_GetTicks64();
    uint64_t dt = t - pt;
    if(dt > 30) dt = 30;
    pt = t;
    if(!arduboy.paused)
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
            if(ImGui::IsKeyDown(ImGuiKey_B)) pinb &= ~0x10;

            arduboy.cpu.data[0x23] = pinb;
            arduboy.cpu.data[0x2c] = pine;
            arduboy.cpu.data[0x2f] = pinf;
        }

#if PROFILING
        dt = 200;
#endif
        bool prev_paused = arduboy.paused;
        arduboy.advance(dt * 1000000000000ull / simulation_slowdown);
        if(arduboy.paused && !prev_paused)
            disassembly_scroll_addr = arduboy.cpu.pc * 2;
    }

    // update framebuffer texture
    {
        void* pixels = nullptr;
        int pitch = 0;
        arduboy.display.filter_pixels();

        SDL_LockTexture(framebuffer_texture, nullptr, &pixels, &pitch);

        uint8_t* bpixels = (uint8_t*)pixels;
        for(int i = 0; i < 64; ++i)
        {
            for(int j = 0; j < 128; ++j)
            {
                auto pi = arduboy.display.filtered_pixels[i * 128 + j];
                *bpixels++ = pi;
                *bpixels++ = pi;
                *bpixels++ = pi;
                *bpixels++ = 255;
            }
        }
        SDL_UnlockTexture(framebuffer_texture);
    }

    SDL_Event event;

    std::string dferr;
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
            dferr = arduboy.load_file(event.drop.file, f);
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

    ImGui::NewFrame();

    const bool enableDocking = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable;
    bool dockspace_created = true;
    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    if(enableDocking)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGuiWindowFlags host_window_flags = 0;
        host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
        host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        if(dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            host_window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Window", nullptr, host_window_flags);
        ImGui::PopStyleVar(3);

        dockspace_created = ImGui::DockBuilderGetNode(dockspace_id) != nullptr;
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, nullptr);
        ImGui::End();
    }

    if(!dockspace_created)
    {
        // default docked layout
        using namespace ImGui;
        ImGuiID c01, c0, c1, c2, c1r01, c1r0, c1r1, c1r2, c2r0, c2r1;
        DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.35f, &c2, &c01);
        DockBuilderSplitNode(c01, ImGuiDir_Left, 0.5f, &c0, &c1);
        DockBuilderSplitNode(c1, ImGuiDir_Down, 0.5f, &c1r2, &c1r01);
        DockBuilderSplitNode(c1r01, ImGuiDir_Down, 0.25f, &c1r1, &c1r0);
        DockBuilderSplitNode(c2, ImGuiDir_Down, 0.75f, &c2r1, &c2r0);
        DockBuilderDockWindow("CPU Data Space", c0);
        DockBuilderDockWindow("Display Internals", c0);
        DockBuilderDockWindow("Display", c1r0);
        DockBuilderDockWindow("Simulation", c1r1);
        DockBuilderDockWindow("Profiler", c1r2);
        DockBuilderDockWindow("Call Stack", c2r0);
        DockBuilderDockWindow("Disassembly", c2r1);
    }

    if(ImGui::BeginMainMenuBar())
    {
        if(ImGui::BeginMenu("Windows"))
        {
            if(ImGui::MenuItem("Display", nullptr, open_display))
                open_display = !open_display;
            if(ImGui::MenuItem("Simulation", nullptr, open_simulation))
                open_simulation = !open_simulation;
            if(ImGui::MenuItem("Disassembly", nullptr, open_disassembly))
                open_disassembly = !open_disassembly;
            if(ImGui::MenuItem("Call Stack", nullptr, open_call_stack))
                open_call_stack = !open_call_stack;
            if(ImGui::MenuItem("CPU Data Space", nullptr, open_data_space))
                open_data_space = !open_data_space;
            if(ImGui::MenuItem("Profiler", nullptr, open_profiler))
                open_profiler = !open_profiler;
            if(ImGui::MenuItem("Display Internals", nullptr, open_display_internals))
                open_display_internals = !open_display_internals;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    window_display(open_display, (void*)framebuffer_texture);
    window_simulation(open_simulation);
    window_disassembly(open_disassembly);
    window_call_stack(open_call_stack);
    window_display_internals(open_display_internals);
    window_profiler(open_profiler);
    window_data_space(open_data_space);

    if(!dferr.empty())
    {
        dropfile_err = dferr;
        ImGui::OpenPopup("File Load Error");
    }

    ImGui::SetNextWindowSize({ 500, 0 });
    if(ImGui::BeginPopupModal("File Load Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(dropfile_err.c_str());
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

    SDL_RenderPresent(renderer);
}

int main(int, char**)
{
#ifdef _WIN32
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

#if PROFILING
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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

    framebuffer_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        128,
        64);

    arduboy.fx.erase_all_data();
    arduboy.reset();

    pt = SDL_GetTicks64();
    done = false;
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, -1, 1);
#else
    while(!done)
        main_loop();
#endif

    if(framebuffer_texture)
    {
        SDL_DestroyTexture(framebuffer_texture);
        framebuffer_texture = nullptr;
    }

    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
