#include "libretro.h"

#include "../absim.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <strstream>
#include <string>
#include <vector>

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

constexpr double FPS = 60.0;

static char const* save_path = nullptr;
static uint64_t need_save_cycle = UINT64_MAX;
static bool need_save = false;

// save interval is 200ms
constexpr uint64_t SAVE_INTERVAL_CYCLES = 16000000 / 5;

static retro_input_descriptor INPUT_DESCS[] =
{
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
    {0, RETRO_DEVICE_NONE, 0, 0, nullptr}
};

static std::unique_ptr<absim::arduboy_t> arduboy;

static void log_default(retro_log_level level, const char* fmt, ...)
{
    FILE* out =
        (level == RETRO_LOG_WARN || level == RETRO_LOG_ERROR ? stderr : stdout);
    va_list va;
    va_start(va, fmt);
    vfprintf(out, fmt, va);
    va_end(va);
}

static retro_log_printf_t         func_log = log_default;
static retro_environment_t        func_env;
static retro_video_refresh_t      func_video;
static retro_audio_sample_t       func_audio_sample;
static retro_audio_sample_batch_t func_audio_batch;
static retro_input_poll_t         func_input_poll;
static retro_input_state_t        func_input_state;

static std::array<uint32_t, 128 * 64> video_buf;

static std::vector<int16_t> audio_buf;

constexpr size_t SAVE_RAM_BYTES =
    sizeof(arduboy->cpu.eeprom) + absim::w25q128_t::DATA_BYTES;
static std::vector<uint8_t> save_buf;

static std::string savedata_filename()
{
    char buf[128];
    uint32_t hash_hi = uint32_t(arduboy->game_hash >> 32);
    uint32_t hash_lo = uint32_t(arduboy->game_hash);
    snprintf(buf, sizeof(buf), "/ardens_%08x%08x.save", hash_hi, hash_lo);
    return std::string(save_path) + buf;
}

static void load_savedata()
{
    if(!save_path) return;

    auto fname = savedata_filename();
    std::ifstream f(fname, std::ios::in | std::ios::binary);
    if(!f.fail())
    {
        func_log(RETRO_LOG_INFO, "Loaded from %s\n", fname.c_str());
        arduboy->load_savedata(f);
    }
    else
    {
        func_log(RETRO_LOG_INFO, "No save file found at %s\n", fname.c_str());
    }
}

//
// REFER TO:
//    https://docs.libretro.com/development/cores/developing-cores
//

void retro_init()
{
    {
        retro_log_callback data;
        if(func_env(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &data))
            func_log = data.log;
    }
    if(!func_env(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_path))
        save_path = nullptr;
    else
        func_log(RETRO_LOG_INFO, "Save path: %s\n", save_path);

    arduboy = std::make_unique<absim::arduboy_t>();
}

void retro_deinit()
{
    arduboy.reset();
}

unsigned retro_api_version()
{
	return RETRO_API_VERSION;
}

void retro_set_environment(retro_environment_t cb) { func_env = cb; }
void retro_set_video_refresh(retro_video_refresh_t cb) { func_video = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { func_audio_sample = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { func_audio_batch = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { func_input_poll = cb; }
void retro_set_input_state(retro_input_state_t cb) { func_input_state = cb; }

void retro_get_system_info(struct retro_system_info* info)
{
    if(!info) return;
    *info = {};
    info->library_name = "Ardens";
    info->library_version = ARDENS_VERSION;
    info->need_fullpath = false;
    info->valid_extensions = "hex|arduboy";
    info->block_extract = true;
}

void retro_get_system_av_info(struct retro_system_av_info* info)
{
    if(!info) return;
    *info = {};
    info->geometry.base_width = 128;
    info->geometry.base_height = 64;
    info->geometry.max_width = 128;
    info->geometry.max_height = 64;
    info->timing.fps = FPS;
    info->timing.sample_rate = 16e6 / double(absim::atmega32u4_t::SOUND_CYCLES);
}

void retro_set_controller_port_device(unsigned port, unsigned device) {}

void retro_reset()
{
    arduboy->reset();
    load_savedata();
}

void retro_run()
{
    func_input_poll();
    bool btn_U = func_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
    bool btn_D = func_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
    bool btn_L = func_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
    bool btn_R = func_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
    bool btn_A = func_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
    bool btn_B = func_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);

    uint8_t pinf = 0xf0;
    uint8_t pine = 0x40;
    uint8_t pinb = 0x10;
    
    std::array<bool, 4> keys = { btn_U, btn_R, btn_D, btn_L, };
    unsigned orientation = 0;
    std::rotate(keys.begin(), keys.begin() + orientation, keys.end());

    if(keys[0]) pinf &= ~0x80;
    if(keys[1]) pinf &= ~0x40;
    if(keys[2]) pinf &= ~0x10;
    if(keys[3]) pinf &= ~0x20;

    if(btn_A) pine &= ~0x40;
    if(btn_B) pinb &= ~0x10;

    arduboy->cpu.data[0x23] = pinb;
    arduboy->cpu.data[0x2c] = pine;
    arduboy->cpu.data[0x2f] = pinf;

    arduboy->frame_bytes_total = 1024;
    arduboy->cpu.enabled_autobreaks = 0;
    arduboy->allow_nonstep_breakpoints = false;
    arduboy->display.enable_filter = true;

    constexpr uint64_t dtps = uint64_t(1e12 / FPS);
    arduboy->advance(dtps);

    for(size_t i = 0; i < video_buf.size(); ++i)
    {
        uint32_t t = arduboy->display.filtered_pixels[i];
        t |= (t << 8) | (t << 16);
        video_buf[i] = t;
    }
    func_video(video_buf.data(), 128, 64, sizeof(uint32_t) * 128);

    audio_buf.clear();
    for(auto sample : arduboy->cpu.sound_buffer)
    {
        audio_buf.push_back(sample);
        audio_buf.push_back(sample);
    }
    arduboy->cpu.sound_buffer.clear();
    func_audio_batch(
        audio_buf.data(),
        audio_buf.size() / 2);

    // Do we need to save?
    if(arduboy->savedata_dirty)
    {
        need_save = true;
        need_save_cycle = arduboy->cpu.cycle_count + SAVE_INTERVAL_CYCLES;
        arduboy->savedata_dirty = false;
    }

    if(save_path && need_save && arduboy->cpu.cycle_count >= need_save_cycle)
    {
        need_save = false;
        std::string fname = savedata_filename();
        std::ofstream f(fname, std::ios::out | std::ios::binary);
        if(!f.fail())
        {
            arduboy->save_savedata(f);
            f.close();
            func_log(RETRO_LOG_INFO, "Saved to %s\n", fname.c_str());
        }
        else
            func_log(RETRO_LOG_ERROR, "Could not save to %s\n", fname.c_str());
    }
}

size_t retro_serialize_size()
{
    static size_t size = size_t(-1);
    if(size == size_t(-1))
    {
        std::ostringstream ss;
        arduboy->save_savestate(ss);
        size = ss.str().size();
        func_log(RETRO_LOG_INFO, "Calculated serialize size: %u\n", (unsigned)size);
    }
    func_log(RETRO_LOG_INFO, "Save size: %u\n", (unsigned)size);
    return size;
}

bool retro_serialize(void* data, size_t size)
{
    std::ostrstream s((char*)data, (std::streamsize)size, std::ios::out | std::ios::binary);
    bool err = arduboy->save_savestate(s);
    if(err) func_log(RETRO_LOG_ERROR, "Error during serialize\n");
    return !err;
}

bool retro_unserialize(const void* data, size_t size)
{
    absim::istrstream s((char const*)data, (std::streamsize)size);
    std::string err = arduboy->load_savestate(s);
    if(err.empty()) return true;
    func_log(RETRO_LOG_ERROR, "%s\n", err.c_str());
    return false;
}

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char* code) {}

bool retro_load_game(const struct retro_game_info* game)
{
    int format = RETRO_PIXEL_FORMAT_XRGB8888;
    if(!func_env(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format))
    {
        func_log(RETRO_LOG_ERROR, "Could not set pixel format\n");
        return false;
    }
    if(!func_env(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, INPUT_DESCS))
    {
        func_log(RETRO_LOG_ERROR, "Could not set input descriptors\n");
        return false;
    }
    if(!game || !game->path || !game->data || game->size == 0)
    {
        func_log(RETRO_LOG_ERROR, "Game format issue\n");
        return false;
    }
    absim::istrstream f((char const*)game->data, game->size);
    std::string err = arduboy->load_file(game->path, f);
    if(!err.empty())
    {
        func_log(RETRO_LOG_ERROR, "%s\n", err.c_str());
        return false;
    }
    load_savedata();
    return true;
}

bool retro_load_game_special(
    unsigned game_type,
    const struct retro_game_info* info, size_t num_info)
{
    return false;
}

void retro_unload_game() {}

unsigned retro_get_region()
{
    return RETRO_REGION_NTSC;
}

void* retro_get_memory_data(unsigned id)
{
    switch(id)
    {
    case RETRO_MEMORY_SYSTEM_RAM:
        return arduboy->cpu.data.data();
    case RETRO_MEMORY_SAVE_RAM:
        // We need to use the save directory, to save both EEPROM and FX saves.
        return nullptr;
    case RETRO_MEMORY_VIDEO_RAM:
        return arduboy->display.ram.data();
    default:
        return nullptr;
    }
}

size_t retro_get_memory_size(unsigned id)
{
    switch(id)
    {
    case RETRO_MEMORY_SYSTEM_RAM:
        return sizeof(arduboy->cpu.data);
    case RETRO_MEMORY_SAVE_RAM:
        return 0;
    case RETRO_MEMORY_VIDEO_RAM:
        return sizeof(arduboy->display.ram);
    default:
        return 0;
    }
}
