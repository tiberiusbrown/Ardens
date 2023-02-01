#include "imgui.h"
//#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include <stdio.h>
#include <SDL.h>

#include <cmath>
#include <algorithm>

#include "absim.hpp"
#include "absim_instructions.hpp"

#define PROFILING 1

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

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

static bool open_disassembly = true;
static bool open_display = true;
static bool open_display_internals = true;
static bool open_data_space = true;
static bool open_profiler = true;
static bool open_simulation = true;

int main(int, char**)
{
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
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("arduboy_sim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
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

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);

    framebuffer_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        128,
        64);

    uint64_t pt = SDL_GetTicks64();

    arduboy.fx.erase_all_data();
    arduboy.reset();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    char const* dropfile_err = nullptr;
    bool done = false;
    while (!done)
    {
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
                if(ImGui::IsKeyDown(ImGuiKey_DownArrow )) pinf &= ~0x10;
                if(ImGui::IsKeyDown(ImGuiKey_LeftArrow )) pinf &= ~0x20;
                if(ImGui::IsKeyDown(ImGuiKey_RightArrow)) pinf &= ~0x40;
                if(ImGui::IsKeyDown(ImGuiKey_UpArrow   )) pinf &= ~0x80;
                if(ImGui::IsKeyDown(ImGuiKey_A         )) pine &= ~0x40;
                if(ImGui::IsKeyDown(ImGuiKey_B         )) pinb &= ~0x10;

                arduboy.cpu.data[0x23] = pinb;
                arduboy.cpu.data[0x2c] = pine;
                arduboy.cpu.data[0x2f] = pinf;
            }

#if PROFILING
            dt = 16;
#endif
            arduboy.advance(dt * 1000000000 / simulation_slowdown);
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

        char const* dferr = nullptr;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
            if(event.type == SDL_DROPFILE)
            {
                dferr = arduboy.load_file(event.drop.file);
                SDL_free(event.drop.file);
                
            }
        }

        ImGui_ImplSDLRenderer_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const bool enableDocking = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable;
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

            ImGuiID dockspace_id = ImGui::GetID("DockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, nullptr);
            ImGui::End();
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
        window_display_internals(open_display_internals);
        window_profiler(open_profiler);
        window_data_space(open_data_space);

        if(dferr)
        {
            dropfile_err = dferr;
            ImGui::OpenPopup("File Load Error");
        }

        if(ImGui::BeginPopupModal("File Load Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted(dropfile_err);
            if(ImGui::Button("OK", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::ShowMetricsWindow();
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
