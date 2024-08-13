#ifdef ARDENS_PLATFORM_SOKOL

#include "common.hpp"

#include <fstream>
#include <cstdio>

#include <imgui.h>
#include <fmt/format.h>

#if defined(__EMSCRIPTEN__)
#define SOKOL_GLES3
#elif defined(__APPLE__)
#define SOKOL_METAL
#elif defined(_WIN32)
#define SOKOL_D3D11
#else
#define SOKOL_GLCORE
#endif

#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#ifndef __EMSCRIPTEN__
#include "sokol/sokol_args.h"
#endif
#include "sokol/sokol_audio.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"
#include "sokol/sokol_imgui.h"

#include "ardens_icon.hpp"

static simgui_desc_t const SIMGUI_DESC = []() {
    simgui_desc_t desc{};
    desc.ini_filename = "imgui.ini";
    desc.no_default_font = true;
    return desc;
}();

static sg_sampler DEFAULT_SAMPLER;

static void app_frame()
{
    frame_logic();

    {
        simgui_frame_desc_t desc{};
        desc.delta_time = sapp_frame_duration();
        desc.dpi_scale = sapp_dpi_scale();
        desc.width = sapp_width();
        desc.height = sapp_height();
        simgui_new_frame(&desc);
    }

    imgui_content();

    sg_pass pass{};
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value = { CLEAR_R, CLEAR_G, CLEAR_B, 1.f };
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);

    simgui_render();

    sg_end_pass();
    sg_commit();
}

static void app_init()
{
    stm_setup();

    {
        sg_desc desc{};
        sg_setup(&desc);
    }

    {
        sg_sampler_desc desc{};
        desc.min_filter = SG_FILTER_LINEAR;
        desc.mag_filter = SG_FILTER_NEAREST;
        desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        DEFAULT_SAMPLER = sg_make_sampler(&desc);
    }

    simgui_setup(&SIMGUI_DESC);

    {
        saudio_desc desc{};
        desc.num_channels = 1;
        desc.sample_rate = AUDIO_FREQ;
        desc.num_packets = 256;
        saudio_setup(&desc);
    }

    init();
    update_pixel_ratio();
    rescale_style();
    rebuild_fonts();

    {
        sg_image_desc desc{};
        desc.width = 128;
        desc.height = 64;
        desc.pixel_format = SG_PIXELFORMAT_RGBA8;
        desc.usage = SG_USAGE_STREAM;

        {
            simgui_image_desc_t idesc{};
            idesc.image = sg_make_image(&desc);
            idesc.sampler = DEFAULT_SAMPLER;
            auto iimg = simgui_make_image(&idesc);
            display_texture = simgui_imtextureid(iimg);
        }

        {
            simgui_image_desc_t idesc{};
            idesc.image = sg_make_image(&desc);
            idesc.sampler = DEFAULT_SAMPLER;
            auto iimg = simgui_make_image(&idesc);
            display_buffer_texture = simgui_imtextureid(iimg);
        }
    }

#ifndef __EMSCRIPTEN__
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
                dropfile_err = fmt::format("Could not open file: \"{}\"", value);
#endif
        }
    }
#endif
}

static void app_event(sapp_event const* e)
{
    simgui_handle_event(e);

    float ipr = 1.f / pixel_ratio;
    if( e->type == SAPP_EVENTTYPE_TOUCHES_BEGAN ||
        e->type == SAPP_EVENTTYPE_TOUCHES_MOVED)
    {
        first_touch = true;
        for(int i = 0; i < e->num_touches; ++i)
        {
            auto& tp = touch_points[e->touches[i].identifier];
            tp = { e->touches[i].pos_x * ipr, e->touches[i].pos_y * ipr };
        }
        sapp_consume_event();
    }
    if(e->type == SAPP_EVENTTYPE_TOUCHES_ENDED)
    {
        for(int i = 0; i < e->num_touches; ++i)
            if(e->touches[i].changed)
                touch_points.erase(e->touches[i].identifier);
        sapp_consume_event();
    }
    if(e->type == SAPP_EVENTTYPE_TOUCHES_CANCELLED)
    {
        touch_points.clear();
        sapp_consume_event();
    }

#if 0
    if(e->type == SAPP_EVENTTYPE_MOUSE_DOWN)
    {
        touch_points.clear();
        touch_points[0] = { e->mouse_x * ipr, e->mouse_y * ipr };
    }
    if(e->type == SAPP_EVENTTYPE_MOUSE_MOVE && !touch_points.empty())
        touch_points[0] = { e->mouse_x * ipr, e->mouse_y * ipr };
    if(e->type == SAPP_EVENTTYPE_MOUSE_UP)
        touch_points.clear();
#endif

#if !defined(__EMSCRIPTEN__) && !defined(ARDENS_DIST) && !defined(ARDENS_FLASHCART)
    if(e->type == SAPP_EVENTTYPE_FILES_DROPPED)
    {
        int n = sapp_get_num_dropped_files();
        for(int i = 0; i < n; ++i)
        {
            char const* fname = sapp_get_dropped_file_path(i);
            std::ifstream f(fname, std::ios::binary);
            std::vector<uint8_t> fdata(
                (std::istreambuf_iterator<char>(f)),
                std::istreambuf_iterator<char>());
            load_file("", fname, fdata.data(), fdata.size());
        }
    }
#endif
}

static void app_cleanup()
{
    shutdown();
#ifndef __EMSCRIPTEN__
    sargs_shutdown();
#endif
    saudio_shutdown();
    simgui_shutdown();
    sg_shutdown();
}

void platform_destroy_texture(texture_t t)
{
    simgui_destroy_image({ (uint32_t)(uintptr_t)t });
}

texture_t platform_create_texture(int w, int h)
{
    sg_image_desc desc{};
    desc.width = w;
    desc.height = h;
    desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    desc.usage = SG_USAGE_STREAM;

    simgui_image_desc_t idesc{};
    idesc.image = sg_make_image(&desc);
    idesc.sampler = DEFAULT_SAMPLER;
    auto t = simgui_make_image(&idesc);

    return (texture_t)(uintptr_t)t.id;
}

void platform_update_texture(texture_t t, void const* data, size_t n)
{
    sg_image_data idata{};
    idata.subimage[0][0] = { data, n };

    auto img = simgui_image_from_imtextureid(t);
    auto desc = simgui_query_image_desc(img);
    sg_update_image(desc.image, &idata);
}

void platform_texture_scale_linear(texture_t t)
{
    // not supported by sokol_gfx
}

void platform_texture_scale_nearest(texture_t t)
{
    // not supported by sokol_gfx
}

void platform_set_clipboard_text(char const* str)
{
    sapp_set_clipboard_string(str);
}

void platform_send_sound()
{
    std::vector<int16_t> buf;
    buf.swap(arduboy.cpu.sound_buffer);

    if(saudio_expect() <= 0)
        return;
    if(saudio_sample_rate() <= 0)
        return;
    if(saudio_channels() <= 0)
        return;
    if(buf.empty())
        return;
    if(saudio_suspended())
        return;

    std::vector<float> sbuf;

    int nc = saudio_channels();
    double const f = double(saudio_sample_rate()) / AUDIO_FREQ;
    size_t ns = size_t(buf.size() * f + 0.5);
    ns = std::min<size_t>(ns, (size_t)saudio_expect());
    sbuf.resize(ns * nc);

    float gain = volume_gain();

    for(size_t i = 0; i < ns; ++i)
    {
        size_t j = size_t(i * f);
        if(j >= buf.size()) j = buf.size() - 1;
        float x = float(buf[j]) * gain;
        for(int c = 0; c < nc; ++c)
            sbuf[i * nc + c] = x;
    }

    if(!sbuf.empty())
        saudio_push(sbuf.data(), (int)ns);

    if(ns < buf.size())
    {
        buf.erase(buf.begin(), buf.begin() + ns);
        buf.swap(arduboy.cpu.sound_buffer);
    }
}

uint64_t platform_get_ms_dt()
{
    static uint64_t dt_rem = 0;
    static uint64_t pt = 0;
    uint64_t dt = stm_laptime(&pt);
    dt += dt_rem;
    uint64_t ms = dt / 1000000;
    dt_rem = dt - ms * 1000000;
    return ms;
}

float platform_pixel_ratio()
{
    return sapp_dpi_scale();
}

void platform_destroy_fonts_texture()
{
    simgui_destroy_image(_simgui.default_font);
    _simgui.font_img = {};
    _simgui.default_font = {};
}

void platform_create_fonts_texture()
{
    unsigned char* font_pixels;
    int font_width, font_height;
    auto& io = ImGui::GetIO();

    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc{};
    img_desc.width = font_width;
    img_desc.height = font_height;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.data.subimage[0][0].ptr = font_pixels;
    img_desc.data.subimage[0][0].size = (size_t)(font_width * font_height) * sizeof(uint32_t);
    img_desc.label = "sokol-imgui-font-image";
    _simgui.font_img = sg_make_image(&img_desc);

    simgui_image_desc_t idesc{};
    idesc.image = _simgui.font_img;
    idesc.sampler = _simgui.font_smp;
    _simgui.default_font = simgui_make_image(&idesc);

    io.Fonts->TexID = simgui_imtextureid(_simgui.default_font);
}

void platform_toggle_fullscreen()
{
#ifdef __EMSCRIPTEN__
    EmscriptenFullscreenChangeEvent e{};
    if(emscripten_get_fullscreen_status(&e) != EMSCRIPTEN_RESULT_SUCCESS)
        return;
    if(e.isFullscreen)
        emscripten_exit_fullscreen();
    else
        emscripten_request_fullscreen("#canvas", true);
#else
    sapp_toggle_fullscreen();
#endif
}

void platform_quit()
{
    sapp_request_quit();
}

void platform_set_title(char const* title)
{
    sapp_set_window_title(title);
}

static std::array<std::vector<uint32_t>, SAPP_MAX_ICONIMAGES> icon_imgs;

static void scale_icon(
    std::vector<uint32_t>& dst,
    std::vector<uint32_t> const& src,
    int w)
{
    dst.resize(src.size() * 4);
    for(int i = 0; i < w; ++i)
    {
        for(int j = 0; j < w; ++j)
        {
            int t = i * w * 4 + j * 2;
            uint32_t x = src[i * w + j];
            dst[t + 0] = x;
            dst[t + 1] = x;
            t += w * 2;
            dst[t + 0] = x;
            dst[t + 1] = x;
        }
    }
}

sapp_desc sokol_main(int argc, char** argv)
{
#ifndef __EMSCRIPTEN__
    {
        sargs_desc d{};
        d.argc = argc;
        d.argv = argv;
        sargs_setup(&d);
    }
#endif

    sapp_desc desc{};
    desc.enable_clipboard = true;
    desc.clipboard_size = (1 << 20); // 1 MB
    desc.high_dpi = true;
    desc.init_cb = app_init;
    desc.event_cb = app_event;
    desc.frame_cb = app_frame;
    desc.cleanup_cb = app_cleanup;
#ifdef __EMSCRIPTEN__
    desc.html5_canvas_name = "canvas";
#else
#ifdef ARDENS_PLAYER
    desc.width = 512;
    desc.height = 256;
#else
    desc.width = 1280;
    desc.height = 720;
#endif
    desc.window_title = preferred_title().c_str();
#ifndef ARDENS_DIST
    desc.enable_dragndrop = true;
    desc.max_dropped_files = 2;
#endif
#endif
#ifdef _WIN32
    desc.win32_console_attach = true;
#endif

#ifndef __EMSCRIPTEN__
    for(int i = 0; i < sargs_num_args(); ++i)
    {
        char const* k = sargs_key_at(i);
        char const* v = sargs_value_at(i);
        if(0 != strcmp(k, "size")) continue;
        int width = desc.width;
        int height = desc.height;
        if(2 == sscanf(v, "%dx%d", &width, &height))
        {
            desc.width = width;
            desc.height = height;
        }
        break;
    }
#endif

    // icon
    if(ardens_icon.bytes_per_pixel == 4 && ardens_icon.width == ardens_icon.height)
    {
        auto& base = icon_imgs[0];
        int w = ardens_icon.width;
        base.resize(w * w);
        memcpy(base.data(), ardens_icon.pixel_data, base.size() * 4);
        for(size_t i = 1; i < icon_imgs.size(); ++i)
            scale_icon(icon_imgs[i], icon_imgs[i - 1], w);
        for(size_t i = 0; i < icon_imgs.size(); ++i)
        {
            auto& image = desc.icon.images[i];
            auto const& icon = icon_imgs[i];
            image.width = w;
            image.height = w;
            image.pixels = { icon.data(), icon.size() * 4 };
            w *= 2;
        }
    }

    return desc;
}

#endif
