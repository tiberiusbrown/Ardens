// Dear ImGui: standalone example application for SDL2 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// Important to understand: SDL_Renderer is an _optional_ component of SDL. We do not recommend you use SDL_Renderer
// because it provides a rather limited API to the end-user. We provide this backend for the sake of completeness.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#include "imgui.h"
//#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include <stdio.h>
#include <SDL.h>

#include <cmath>
#include <algorithm>

#include <fmt/format.h>

#include "absim.hpp"
#include "absim_instructions.hpp"

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

static SDL_Texture* framebuffer_texture;
absim::arduboy_t arduboy;

int profiler_selected_hotspot = -1;
int disassembly_scroll_addr = -1;
bool profiler_cycle_counts = false;

void window_disassembly(bool& open);
void window_profiler(bool& open);
void window_display(bool& open, void* tex);
void window_display_internals(bool& open);
void window_data_space(bool& open);

static bool open_disassembly = true;
static bool open_display = true;
static bool open_display_internals = true;
static bool open_data_space = true;
static bool open_profiler = true;

// Main code
int main(int, char**)
{
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to the latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);

    // Setup SDL_Renderer instance
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
    {
        SDL_Log("Error creating SDL_Renderer!");
        return 0;
    }
    //SDL_RendererInfo info;
    //SDL_GetRendererInfo(renderer, &info);
    //SDL_Log("Current SDL_Renderer: %s", info.name);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    ImGuiStyle& style = ImGui::GetStyle();
    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    framebuffer_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        128,
        64);

    uint64_t pt = SDL_GetTicks64();

    arduboy.reset();

    // Main loop
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

            arduboy.advance(dt * 1000000000);
            //arduboy.advance(dt * 10000000, 0.01);
            //arduboy.advance(dt * 1000000, 0.001);
            //arduboy.advance(dt * 1000);
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

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
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

        // Start the Dear ImGui frame
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
            if(ImGui::MenuItem("Display", nullptr, open_display))
                open_display = !open_display;
            if(ImGui::MenuItem("Disassembly", nullptr, open_disassembly))
                open_disassembly = !open_disassembly;
            if(ImGui::MenuItem("CPU Data Space", nullptr, open_data_space))
                open_data_space = !open_data_space;
            if(ImGui::MenuItem("Profiler", nullptr, open_profiler))
                open_profiler = !open_profiler;
            if(ImGui::MenuItem("Display Internals", nullptr, open_display_internals))
                open_display_internals = !open_display_internals;
            ImGui::EndMainMenuBar();
        }

        window_disassembly(open_disassembly);
        window_display(open_display, (void*)framebuffer_texture);
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

        // Rendering
        ImGui::Render();

        // Update and Render additional Platform Windows
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

    // Cleanup
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
