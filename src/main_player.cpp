#if defined(__EMSCRIPTEN__)
#define SOKOL_GLES2
#elif defined(__APPLE__)
#define SOKOL_METAL
#elif defined(_WIN32)
#define SOKOL_D3D11
#else
#define SOKOL_GLCORE33
#endif


#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_audio.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_gl.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"

#include <fstream>
#include <memory>
#include <strstream>

#include <stdio.h>

#include "absim.hpp"

constexpr int AUDIO_FREQ = 16000000 / absim::atmega32u4_t::SOUND_CYCLES;

static std::unique_ptr<absim::arduboy_t> arduboy;
static uint64_t pt = 0;

static bool btn_up;
static bool btn_down;
static bool btn_left;
static bool btn_right;
static bool btn_a;
static bool btn_b;

static int num_pixel_history = 1;

static sg_image framebuffer;
static uint8_t pixels[128 * 64 * 4];

extern "C" void postsyncfs() {}
extern "C" int setparam(char const* name, char const* value)
{
    if(!name || !value) return 0;
    std::string p(name);
    if(p == "g")
    {
        int g = 1;
        if(*value == '2') g = 2;
        if(*value == '3') g = 3;
        num_pixel_history = g;
        return 1;
    }
    return 0;
}

extern "C" int load_file(char const* filename, uint8_t const* data, size_t size)
{
    std::istrstream f((char const*)data, size);
    auto err = arduboy->load_file(filename, f);
    if(!err.empty())
    {
        printf("Error while loading \"%s\": %s\n", filename, err.c_str());
        return 0;
    }
    printf("Successfully loaded \"%s\"\n", filename);
    return 1;
}

static void app_init()
{
    arduboy = std::make_unique<absim::arduboy_t>();
    stm_setup();

    {
        sg_desc desc{};
        desc.context = sapp_sgcontext();
        sg_setup(&desc);
    }

    {
        sg_image_desc desc{};
        desc.width = 128;
        desc.height = 64;
        desc.pixel_format = SG_PIXELFORMAT_RGBA8;
        desc.min_filter = SG_FILTER_LINEAR;
        desc.mag_filter = SG_FILTER_NEAREST;
        desc.usage = SG_USAGE_STREAM;
        framebuffer = sg_make_image(&desc);
    }

    {
        sgl_desc_t desc{};
        sgl_setup(&desc);
    }

    {
        saudio_desc desc{};
        desc.num_channels = 1;
        desc.sample_rate = AUDIO_FREQ;
        desc.packet_frames = 2048;
        saudio_setup(&desc);
    }

    printf("%s\n", "arduboy_sim_player " ABSIM_VERSION);
    printf("audio: channels=%d sample_rate=%d\n", saudio_channels(), saudio_sample_rate());
}

static void app_event(sapp_event const* e)
{
    if(e->type == SAPP_EVENTTYPE_KEY_DOWN || e->type == SAPP_EVENTTYPE_KEY_UP)
    {
        bool down = (e->type == SAPP_EVENTTYPE_KEY_DOWN);
        if(e->key_code == SAPP_KEYCODE_UP) btn_up = down;
        if(e->key_code == SAPP_KEYCODE_DOWN) btn_down = down;
        if(e->key_code == SAPP_KEYCODE_LEFT) btn_left = down;
        if(e->key_code == SAPP_KEYCODE_RIGHT) btn_right = down;
        if(e->key_code == SAPP_KEYCODE_A) btn_a = down;
        if(e->key_code == SAPP_KEYCODE_B || e->key_code == SAPP_KEYCODE_S)
            btn_b = down;
    }

#ifndef __EMSCRIPTEN__
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
            load_file(fname, fdata.data(), fdata.size());
        }
    }
#endif
}

// resample sound to target freq using nearest sampling
static void process_sound()
{
    std::vector<int16_t> buf;
    buf.swap(arduboy->cpu.sound_buffer);
    if(buf.empty())
        return;
    if(saudio_expect() <= 0)
        return;
    if(saudio_sample_rate() <= 0)
        return;
    if(saudio_suspended())
        return;

    std::vector<float> sbuf;

    double const f = double(saudio_sample_rate()) / AUDIO_FREQ;
    sbuf.resize(size_t(buf.size() * f + 0.5));
    if(sbuf.size() > saudio_expect())
        sbuf.resize(saudio_expect());

    constexpr float SOUND_GAIN = 1.f / 32768;
    for(size_t i = 0; i < sbuf.size(); ++i)
    {
        size_t j = size_t(i * f);
        if(j >= buf.size()) j = buf.size() - 1;
        sbuf[i] = float(buf[j]) * SOUND_GAIN;
    }

    if(!sbuf.empty())
        saudio_push(sbuf.data(), (int)sbuf.size());
}

static void app_frame()
{
    if(arduboy->cpu.decoded && !arduboy->paused)
    {
        uint64_t dt = stm_laptime(&pt);
        double us = stm_us(dt);
        uint64_t ps = (uint64_t)us * 1000000;

        uint8_t pinf = 0xf0;
        uint8_t pine = 0x40;
        uint8_t pinb = 0x10;

        if(btn_down) pinf &= ~0x10;
        if(btn_left) pinf &= ~0x20;
        if(btn_right) pinf &= ~0x40;
        if(btn_up) pinf &= ~0x80;
        if(btn_a) pine &= ~0x40;
        if(btn_b) pinb &= ~0x10;

        arduboy->cpu.data[0x23] = pinb;
        arduboy->cpu.data[0x2c] = pine;
        arduboy->cpu.data[0x2f] = pinf;

        arduboy->frame_bytes_total = (num_pixel_history == 1 ? 1024 : 0);
        arduboy->cpu.enable_stack_break = false;
        arduboy->allow_nonstep_breakpoints = false;
        arduboy->advance(ps);

        {
            arduboy->display.num_pixel_history = num_pixel_history;
            arduboy->display.filter_pixels();
            uint8_t const* p = arduboy->display.filtered_pixels.data();
            for(int i = 0; i < 128 * 64; ++i)
            {
                pixels[i * 4 + 0] = p[i];
                pixels[i * 4 + 1] = p[i];
                pixels[i * 4 + 2] = p[i];
                pixels[i * 4 + 3] = 255;
            }
            sg_image_data data{};
            data.subimage[0][0] = SG_RANGE(pixels);
            sg_update_image(framebuffer, &data);
        }

        process_sound();
    }

    sgl_defaults();
    sgl_enable_texture();
    sgl_texture(framebuffer);
    sgl_ortho(0.f, sapp_widthf(), sapp_heightf(), 0.f, -1.f, 1.f);

    float x0 = 0.f;
    float y0 = 0.f;
    float w = sapp_widthf();
    float h = sapp_heightf();
    if(w < h * 2.f)
        h = w * 0.5f;
    else if(w > h * 2.f)
        w = h * 2.f;
    x0 += (sapp_widthf() - w) * 0.5f;
    y0 += (sapp_heightf() - h) * 0.5f;
    float x1 = x0 + w;
    float y1 = y0 + h;

    sgl_begin_quads();
    sgl_v2f_t2f(x0, y0, 0.f, 0.f);
    sgl_v2f_t2f(x0, y1, 0.f, 1.f);
    sgl_v2f_t2f(x1, y1, 1.f, 1.f);
    sgl_v2f_t2f(x1, y0, 1.f, 0.f);
    sgl_end();

    sg_pass_action action{};
    action.colors[0].action = SG_ACTION_CLEAR;
    sg_begin_default_passf(&action, sapp_widthf(), sapp_heightf());

    sgl_draw();

    sg_end_pass();
    sg_commit();
}

static void app_cleanup()
{
    saudio_shutdown();
    sgl_shutdown();
    sg_destroy_image(framebuffer);
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char** argv)
{
    sapp_desc desc{};
    desc.high_dpi = true;
    desc.init_cb = app_init;
    desc.event_cb = app_event;
    desc.frame_cb = app_frame;
    desc.cleanup_cb = app_cleanup;
#ifndef __EMSCRIPTEN__
    desc.width = 512;
    desc.height = 256;
    desc.window_title = "arduboy_sim_player";
    desc.enable_dragndrop = true;
    desc.max_dropped_files = 2;
#endif
    return desc;
}
