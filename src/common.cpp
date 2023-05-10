#include "common.hpp"

#include <fstream>
#include <strstream>

#include <imgui.h>
#include <implot.h>

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

extern unsigned char const ProggyVector[198188];

texture_t display_texture = nullptr;
texture_t display_buffer_texture = nullptr;
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

bool layout_done = false;
bool settings_loaded = false;
#ifdef __EMSCRIPTEN__
bool fs_ready = false;
#else
bool fs_ready = true;
#endif

extern "C" int load_file(char const* filename, uint8_t const* data, size_t size)
{
    std::istrstream f((char const*)data, size);
    dropfile_err = arduboy->load_file(filename, f);
    if(dropfile_err.empty()) load_savedata();
    return 0;
}

bool update_pixel_ratio()
{
    float ratio = platform_pixel_ratio();
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
    io.Fonts->Clear();
    io.Fonts->AddFontFromMemoryTTF(
        (void*)ProggyVector, sizeof(ProggyVector), 13.f * pixel_ratio, &cfg);
#ifdef __EMSCRIPTEN__
    //io.FontGlobalScale = 1.f / pixel_ratio;
#endif
}

static void rebuild_fonts()
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

void init()
{
#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.mkdir('/offline');
    FS.mount(IDBFS, {}, '/offline');
    FS.syncfs(true, function(err) { ccall('postsyncfs', 'v'); });
    );
#endif

    arduboy = std::make_unique<absim::arduboy_t>();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

#ifndef ABSIM_NO_SAVED_SETTINGS
    init_settings();
#endif

    ImGuiIO& io = ImGui::GetIO();
#if !defined(__EMSCRIPTEN__) && !defined(ABSIM_NO_SAVED_SETTINGS)
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
    settings_loaded = true;
#endif

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

#if defined(__EMSCRIPTEN__) || defined(ABSIM_NO_SAVED_SETTINGS)
    io.IniFilename = nullptr;
#endif

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    default_style = style;

    arduboy->fx.erase_all_data();
    arduboy->reset();
    arduboy->fx.min_page = 0xffff;
    arduboy->fx.max_page = 0xffff;
}

void frame_logic()
{
    ImGuiIO& io = ImGui::GetIO();

#ifdef __EMSCRIPTEN__
    if(done) emscripten_cancel_main_loop();
#endif

    // advance simulation
    uint64_t dt = platform_get_ms_dt();
    if(dt > 30) dt = 30;
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
        arduboy->frame_bytes_total = 1024;
        arduboy->cpu.enable_stack_break = settings.enable_stack_breaks;
        arduboy->allow_nonstep_breakpoints =
            arduboy->break_step == 0xffffffff || settings.enable_step_breaks;
        arduboy->display.enable_filter = settings.display_auto_filter;

        if(gif_recording)
        {
            uint64_t ms = 20 - gif_ms_rem;
            while(dt >= ms)
            {
                arduboy->advance(ms * 1000000000000ull / simulation_slowdown);
                send_gif_frame(2, recording_pixels(false));
                dt -= ms;
                ms = 20;
                gif_ms_rem = 0;
            }
            gif_ms_rem = dt;
        }
        if(dt > 0)
            arduboy->advance(dt * 1000000000000ull / simulation_slowdown);

        check_save_savedata();

        if(arduboy->paused && !prev_paused)
            disassembly_scroll_addr = arduboy->cpu.pc * 2;
        if(!settings.enable_stack_breaks)
            arduboy->cpu.stack_overflow = false;

        // consume sound buffer
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
#ifndef ABSIM_NO_SNAPSHOTS
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
#endif
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
#endif
    }

    if(!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_P, false))
    {
        settings.fullzoom = !settings.fullzoom;
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

#ifndef ABSIM_NO_GUI
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
#endif

#ifndef ABSIM_NO_DEBUGGER
    if(settings.fullzoom)
        view_player();
    else
        view_debugger();
#else
    view_player();
#endif

#ifndef ABSIM_NO_GUI
    settings_modal();
#endif

#ifndef ABSIM_NO_GUI
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
#endif

#ifndef ABSIM_NO_GUI
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
#endif

#ifndef ABSIM_NO_GUI
#if PROFILING
    ImGui::ShowMetricsWindow();
#endif
    //ImGui::ShowDemoWindow();
#endif

    ImGui::Render();

    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}
