#include <SDL2/SDL_main.h>
#include <SDL2/SDL.h>

#include <absim.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <winrt/Windows.Storage.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <strstream>

extern "C" uint8_t game_arduboy[];
extern "C" uint32_t game_arduboy_size;

static std::unique_ptr<absim::arduboy_t> arduboy;

static SDL_AudioDeviceID audio_device;
static SDL_AudioSpec audio_spec;
constexpr uint32_t AUDIO_FREQ = 16000000 / absim::atmega32u4_t::SOUND_CYCLES;
constexpr uint32_t MAX_AUDIO_LATENCY_SAMPLES = 512;

constexpr uint32_t SAVE_INTERVAL_MS = 1000;

static SDL_Texture* display_texture;
static bool need_save = false;
static uint64_t need_save_time = 0;

static std::vector<int16_t> audio_buf;

static void audio_callback(void* user, uint8_t* data, int n)
{
    SDL_LockAudioDevice(audio_device);
    auto& buf = audio_buf;
    size_t bufn = buf.size() * 2;
    if(bufn < (size_t)n)
    {
        memcpy(data, buf.data(), bufn);
        memset(data + bufn, 0, (size_t)n - bufn);
        buf.clear();
    }
    else
    {
        memcpy(data, buf.data(), n);
        buf.erase(buf.begin(), buf.begin() + n / 2);
    }
    SDL_UnlockAudioDevice(audio_device);
}

static void send_sound()
{
    auto& buf = arduboy->cpu.sound_buffer;
    constexpr size_t SAMPLE_SIZE = sizeof(buf[0]);
    size_t num_bytes = buf.size() * SAMPLE_SIZE;
    uint32_t queued_bytes = SDL_GetQueuedAudioSize(audio_device);
    constexpr uint32_t BUFFER_BYTES = 2048 * SAMPLE_SIZE;
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
    buf.clear();
}

static std::wstring savedata_filename()
{
    Windows::Storage::StorageFolder^ folder = Windows::Storage::ApplicationData::Current->LocalFolder;
    Platform::String^ path = folder->Path;
    std::wstring spath(path->Data());
    return spath + L"\\game.save";
}

static void check_save_savedata()
{
    if(arduboy->savedata_dirty)
    {
        need_save = true;
        need_save_time = SDL_GetTicks64() + SAVE_INTERVAL_MS;
        arduboy->savedata_dirty = false;
    }

    if(need_save && SDL_GetTicks64() >= need_save_time)
    {
        need_save = false;
        auto fname = savedata_filename();
        std::ofstream f(fname, std::ios::out | std::ios::binary);
        if(!f.fail())
        {
            arduboy->save_savedata(f);
            f.close();
        }
    }
}

static void setup_arduboy()
{
    if(!arduboy)
        return;
    arduboy->fx.erase_all_data();
    std::istrstream ss((char const*)game_arduboy, (int)game_arduboy_size);
    (void)arduboy->load_file("game.arduboy", ss);

    // load save data if any exists
    auto savepath = savedata_filename();
    std::ifstream f(savepath, std::ios::in | std::ios::binary);
    if(f.good())
        arduboy->load_savedata(f);
}

static void load_snapshot()
{
    Windows::Storage::StorageFolder^ folder = Windows::Storage::ApplicationData::Current->LocalFolder;
    Platform::String^ path = folder->Path;
    std::wstring spath(path->Data());
    spath += L"\\suspended.snapshot";
    std::wstring savepath = savedata_filename();
    do
    {
        // compare mod times
        std::filesystem::path psave(savepath);
        std::filesystem::path psnap(spath);
        std::error_code ec{};
        auto tsave = std::filesystem::last_write_time(psave, ec);
        if(ec) break;
        auto tsnap = std::filesystem::last_write_time(psnap, ec);
        if(ec) break;
        if(tsnap > tsave) break;
        // snapshot is older than save data
        setup_arduboy();
        return;
    } while(0);
    std::ifstream f(spath, std::ios::in | std::ios::binary);
    if(f.good())
    {
        auto err = arduboy->load_snapshot(f);
        if(!err.empty())
            setup_arduboy();
    }
    else
    {
        setup_arduboy();
    }
}

static int event_filter(void* user, SDL_Event* e)
{
    switch(e->type)
    {
    case SDL_APP_WILLENTERBACKGROUND:
    {
        // on WinRT, this runs in the main thread
        Windows::Storage::StorageFolder^ folder = Windows::Storage::ApplicationData::Current->LocalFolder;
        Platform::String^ path = folder->Path;
        std::wstring spath(path->Data());
        std::ofstream f(spath + L"\\suspended.snapshot", std::ios::out | std::ios::binary);
        if(f.good())
            arduboy->save_snapshot(f);
        break;
    }
    case SDL_APP_WILLENTERFOREGROUND:
    {
        // on WinRT, this runs in the main thread
        load_snapshot();
        break;
    }
    default:
        break;
    }
    return 0;
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
    load_snapshot();

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
        return 1;

    SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");

    SDL_AddEventWatch(event_filter, nullptr);

    {
        SDL_AudioSpec desired;
        memset(&desired, 0, sizeof(desired));
        desired.freq = AUDIO_FREQ;
        desired.format = AUDIO_S16SYS;
        desired.channels = 1;
        desired.samples = MAX_AUDIO_LATENCY_SAMPLES;
        desired.callback = audio_callback;
        audio_device = SDL_OpenAudioDevice(
            nullptr, 0,
            &desired,
            &audio_spec,
            0);
        SDL_PauseAudioDevice(audio_device, 0);
    }

    //if(SDL_CreateWindowAndRenderer(mode->w, mode->h, SDL_WINDOW_FULLSCREEN, &window, &renderer) != 0)

    window = SDL_CreateWindow(
        arduboy->title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        512, 256,
        SDL_WINDOW_RESIZABLE);
    if(!window)
        return 1;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if(!renderer)
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
            //if(e.type == SDL_QUIT)
            //    done = true;
            //if((e.type == SDL_KEYDOWN) && (e.key.keysym.sym == SDLK_ESCAPE))
            //    done = true;
            //if(e.type == SDL_WINDOWEVENT && e.window.windowID == SDL_GetWindowID(window) && e.window.event == SDL_WINDOWEVENT_CLOSE)
            //    done = true;
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

            if( ImGui::IsKeyDown(keys[0]) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadLStickUp) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadRStickUp) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadDpadUp))
                pinf &= ~0x80;
            if( ImGui::IsKeyDown(keys[1]) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadLStickRight) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadRStickRight) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadDpadRight))
                pinf &= ~0x40;
            if( ImGui::IsKeyDown(keys[2]) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadLStickDown) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadRStickDown) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown))
                pinf &= ~0x10;
            if( ImGui::IsKeyDown(keys[3]) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadLStickLeft) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadRStickLeft) ||
                ImGui::IsKeyDown(ImGuiKey_GamepadDpadLeft))
                pinf &= ~0x20;

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
            uint64_t dtps = std::min<uint64_t>(dt, 50) * MS_TO_PS;
            while(dtps > 0)
            {
                uint64_t ps = MS_TO_PS;
                if(ps < dtps)
                    ps = dtps;
                arduboy->advance(ps);
                dtps -= ps;

                if(audio_spec.callback)
                {
                    SDL_LockAudioDevice(audio_device);
                    audio_buf.insert(
                        audio_buf.end(),
                        arduboy->cpu.sound_buffer.begin(),
                        arduboy->cpu.sound_buffer.end());
                    SDL_UnlockAudioDevice(audio_device);
                    arduboy->cpu.sound_buffer.clear();
                }
                else
                {
                    send_sound();
                }
            }

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

        {
            int ww = 0;
            int wh = 0;
            SDL_GetWindowSizeInPixels(window, &ww, &wh);
            int w = 128;
            int h = 64;
            while(w + 128 <= ww && h + 64 <= wh)
                w += 128, h += 64;
            int x = (ww - w) / 2;
            int y = (wh - h) / 2;
            ImVec2 a = { float(x), float(y) };
            ImVec2 b = { a.x + w, a.y + h };
            auto* d = ImGui::GetBackgroundDrawList();
            d->AddImage(display_texture, a, b);
            int numh = 128;
            int numv = 64;
            float const inc = float(w) / numh;
            float const line_thickness = inc * (1.f / 16);
            float const off = line_thickness * 0.5f;
            ImU32 line_color = IM_COL32(0, 0, 0, 128);
            for(int i = 0; i <= numh; ++i)
            {
                ImVec2 p1 = { a.x + inc * i - off, a.y };
                ImVec2 p2 = { a.x + inc * i + off, b.y };
                d->PathRect(p1, p2);
                d->PathFillConvex(line_color);
            }
            for(int i = 0; i <= numv; ++i)
            {
                ImVec2 p1 = { a.x, a.y + inc * i - off };
                ImVec2 p2 = { b.x, a.y + inc * i + off };
                d->PathRect(p1, p2);
                d->PathFillConvex(line_color);
            }
        }

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
