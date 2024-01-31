#include <SDL2/SDL_main.h>
#include <SDL2/SDL.h>

#include <absim.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <array>
#include <strstream>

extern "C" uint8_t game_arduboy[];
extern "C" uint32_t game_arduboy_size;

static std::unique_ptr<absim::arduboy_t> arduboy;

static SDL_AudioDeviceID audio_device;
static SDL_AudioSpec audio_spec;
constexpr uint32_t AUDIO_FREQ = 16000000 / absim::atmega32u4_t::SOUND_CYCLES;
constexpr uint32_t MAX_AUDIO_LATENCY_SAMPLES = 2048;

static SDL_Texture* display_texture;

static void send_sound()
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
        float gain = 1.75f;
        for(auto& b : buf)
            b = int16_t(std::clamp<float>(gain * b, INT16_MIN, INT16_MAX));
        SDL_QueueAudio(
            audio_device,
            buf.data(),
            int(buf.size() * sizeof(buf[0])));
    }
}

static void check_save_savedata()
{

}

int main(int argc, char* argv[])
{
    SDL_DisplayMode const* mode = nullptr;
    int display_count = 0;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Event e;
    bool done = false;

    arduboy = std::make_unique<absim::arduboy_t>();
    if(!arduboy)
        return 1;
    {
        std::istrstream ss((char const*)game_arduboy, (int)game_arduboy_size);
        auto error = arduboy->load_file("game.arduboy", ss);
        if(!error.empty())
            return 1;
    }

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
        return 1;

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

    //if(SDL_CreateWindowAndRenderer(mode->w, mode->h, SDL_WINDOW_FULLSCREEN, &window, &renderer) != 0)
    if(SDL_CreateWindowAndRenderer(512, 256, SDL_WINDOW_RESIZABLE, &window, &renderer) != 0)
        return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    display_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        128,
        64);
    SDL_SetTextureScaleMode(display_texture, SDL_ScaleModeNearest);

    uint64_t pt = SDL_GetTicks64();

    while(!done)
    {
        uint64_t t = SDL_GetTicks64();
        uint64_t dt = t - pt;
        pt = t;

        while(SDL_PollEvent(&e))
        {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if(e.type == SDL_QUIT)
                done = true;
            if((e.type == SDL_KEYDOWN) && (e.key.keysym.sym == SDLK_ESCAPE))
                done = true;
            if(e.type == SDL_WINDOWEVENT && e.window.windowID == SDL_GetWindowID(window) && e.window.event == SDL_WINDOWEVENT_CLOSE)
                done = true;
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        // advance arduboy
        if(arduboy && !arduboy->paused)
        {
            uint8_t pinf = 0xf0;
            uint8_t pine = 0x40;
            uint8_t pinb = 0x10;
            std::array<ImGuiKey, 4> keys =
            {
                ImGuiKey_UpArrow,
                ImGuiKey_RightArrow,
                ImGuiKey_DownArrow,
                ImGuiKey_LeftArrow,
            };

            if(ImGui::IsKeyDown(keys[0])) pinf &= ~0x80;
            if(ImGui::IsKeyDown(keys[1])) pinf &= ~0x40;
            if(ImGui::IsKeyDown(keys[2])) pinf &= ~0x10;
            if(ImGui::IsKeyDown(keys[3])) pinf &= ~0x20;

            if( ImGui::IsKeyDown(ImGuiKey_A) ||
                ImGui::IsKeyDown(ImGuiKey_Z) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown))
                pine &= ~0x40;
            if( ImGui::IsKeyDown(ImGuiKey_B) ||
                ImGui::IsKeyDown(ImGuiKey_S) ||
                ImGui::IsKeyDown(ImGuiKey_X) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadFaceRight))
                pinb &= ~0x10;

            arduboy->cpu.data[0x23] = pinb;
            arduboy->cpu.data[0x2c] = pine;
            arduboy->cpu.data[0x2f] = pinf;

            arduboy->frame_bytes_total = 1024;
            arduboy->allow_nonstep_breakpoints = false;
            arduboy->display.enable_filter = true;
            arduboy->display.enable_current_limiting = true;
            arduboy->display.ref_segment_current = 0.195f;
            arduboy->display.current_limit_slope = 0.75f;

            constexpr uint64_t MS_TO_PS = 1000000000ull;
            uint64_t dtps = dt * MS_TO_PS;
            if(dtps > 0)
                arduboy->advance(dtps);

            send_sound();
            arduboy->cpu.sound_buffer.clear();

            check_save_savedata();

            std::vector<uint8_t> pixels;
            pixels.resize(128 * 64 * 4);
            for(size_t i = 0; i < arduboy->display.filtered_pixels.size(); ++i)
            {
                pixels[i * 4 + 0] = arduboy->display.filtered_pixels[i];
                pixels[i * 4 + 1] = arduboy->display.filtered_pixels[i];
                pixels[i * 4 + 2] = arduboy->display.filtered_pixels[i];
                pixels[i * 4 + 3] = 255;
            }

            SDL_UpdateTexture(display_texture, nullptr, pixels.data(), 128 * 4);
        }

        ImGui::NewFrame();

        auto* d = ImGui::GetBackgroundDrawList();
        d->AddImage(display_texture, { 0, 0 }, { 128, 64 });

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
        SDL_RenderClear(renderer);

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

        SDL_RenderPresent(renderer);
    }

    if(display_texture)
    {
        SDL_DestroyTexture(display_texture);
        display_texture = nullptr;
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_CloseAudioDevice(audio_device);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
