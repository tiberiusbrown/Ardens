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

#include "settings.hpp"

#define PROFILING 0

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

extern unsigned char const ProggyVector[198188];

static SDL_Texture* framebuffer_texture;
absim::arduboy_t arduboy;

int profiler_selected_hotspot = -1;
int disassembly_scroll_addr = -1;
bool scroll_addr_to_top = false;
int simulation_slowdown = 1000;

void window_disassembly(bool& open);
void window_profiler(bool& open);
void window_display(bool& open, void* tex);
void window_display_internals(bool& open);
void window_data_space(bool& open);
void window_simulation(bool& open);
void window_call_stack(bool& open);
void window_symbols(bool& open);
void window_globals(bool& open);

static uint64_t pt;
static std::string dropfile_err;
static bool done = false;
static bool layout_done = false;
#ifdef __EMSCRIPTEN__
static bool settings_loaded = false;
static bool fs_ready = false;
#else
static bool fs_ready = true;
#endif

static SDL_Window* window;
static SDL_Renderer* renderer;

static float pixel_ratio = 1.f;
static ImGuiStyle default_style;

extern "C" void postsyncfs()
{
    //printf("postsyncfs\n");
    fs_ready = true;
}

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
            if(ImGui::IsKeyDown(ImGuiKey_B) || ImGui::IsKeyDown(ImGuiKey_S)) pinb &= ~0x10;

            arduboy.cpu.data[0x23] = pinb;
            arduboy.cpu.data[0x2c] = pine;
            arduboy.cpu.data[0x2f] = pinf;
        }

#if PROFILING
        dt = 200;
#endif
        bool prev_paused = arduboy.paused;
        arduboy.allow_nonstep_breakpoints =
            arduboy.break_step == 0xffffffff || settings.enable_step_breaks;
        arduboy.advance(dt * 1000000000000ull / simulation_slowdown);
        if(arduboy.paused && !prev_paused)
            disassembly_scroll_addr = arduboy.cpu.pc * 2;
    }
    else
    {
        arduboy.break_step = 0xffffffff;
    }

    // update framebuffer texture
    {
        void* pixels = nullptr;
        int pitch = 0;
        arduboy.display.num_pixel_history = settings.num_pixel_history;
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
    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    if(fs_ready && enableDocking)
    {
#ifdef __EMSCRIPTEN__
        if(!settings_loaded)
        {
            ImGui::LoadIniSettingsFromDisk("/offline/imgui.ini");
            settings_loaded = true;
        }
        if(ImGui::GetIO().WantSaveIniSettings)
        {
            ImGui::GetIO().WantSaveIniSettings = false;
            ImGui::SaveIniSettingsToDisk("/offline/imgui.ini");
            EM_ASM(
                FS.syncfs(function(err) { assert(!err); });
            );
        }
#endif

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

        bool dockspace_created = ImGui::DockBuilderGetNode(dockspace_id) != nullptr;
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, nullptr);
        ImGui::End();

        if(!dockspace_created && !layout_done)
        {
            // default docked layout
            using namespace ImGui;
            ImGuiID c01, c01u, c01d, c0, c1, c2, c1r01, c1r0, c1r1, c1r2, c2r0, c2r1;
            DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.35f, &c2, &c01);
            DockBuilderSplitNode(c01, ImGuiDir_Up, 0.65f, &c01u, &c01d);
            DockBuilderSplitNode(c01u, ImGuiDir_Left, 0.5f, &c0, &c1);
            DockBuilderSplitNode(c1, ImGuiDir_Down, 0.25f, &c1r2, &c1r01);
            DockBuilderSplitNode(c1r01, ImGuiDir_Down, 0.25f, &c1r1, &c1r0);
            DockBuilderSplitNode(c2, ImGuiDir_Down, 0.75f, &c2r1, &c2r0);
            DockBuilderDockWindow("CPU Data Space", c0);
            DockBuilderDockWindow("Display Internals", c0);
            DockBuilderDockWindow("Symbols", c0);
            DockBuilderDockWindow("Display", c1r0);
            DockBuilderDockWindow("Simulation", c1r1);
            DockBuilderDockWindow("Profiler", c1r2);
            DockBuilderDockWindow("Call Stack", c2r0);
            DockBuilderDockWindow("Disassembly", c2r1);
            DockBuilderDockWindow("Globals", c01d);
        }
        layout_done = true;
    }

    if(layout_done)
    {

        if(ImGui::BeginMainMenuBar())
        {
            if(ImGui::BeginMenu("Windows"))
            {
                ImGui::MenuItem("Display", nullptr, &settings.open_display);
                ImGui::MenuItem("Simulation", nullptr, &settings.open_simulation);
                ImGui::MenuItem("Disassembly", nullptr, &settings.open_disassembly);
                ImGui::MenuItem("Symbols", nullptr, &settings.open_symbols);
                ImGui::MenuItem("Globals", nullptr, &settings.open_globals);
                ImGui::MenuItem("Call Stack", nullptr, &settings.open_call_stack);
                ImGui::MenuItem("CPU Data Space", nullptr, &settings.open_data_space);
                ImGui::MenuItem("Profiler", nullptr, &settings.open_profiler);
                ImGui::MenuItem("Display Internals", nullptr, &settings.open_display_internals);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        window_display(settings.open_display, (void*)framebuffer_texture);
        window_simulation(settings.open_simulation);
        window_disassembly(settings.open_disassembly);
        window_symbols(settings.open_symbols);
        window_globals(settings.open_globals);
        window_call_stack(settings.open_call_stack);
        window_display_internals(settings.open_display_internals);
        window_profiler(settings.open_profiler);
        window_data_space(settings.open_data_space);

    }

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
#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.mkdir('/offline');
        FS.mount(IDBFS, {}, '/offline');
        FS.syncfs(true, function(err) { ccall('postsyncfs', 'v'); });
    );
#endif

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

    init_settings();

    ImGuiIO& io = ImGui::GetIO();
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
