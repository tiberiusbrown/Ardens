#ifndef __EMSCRIPTEN__

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "common.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

#include "sokol/sokol_args.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace
{
constexpr uint64_t MS_PS = 1'000'000'000ull;

char const* capture_serial_value()
{
    if(sargs_exists("captureserial"))
        return sargs_value("captureserial");
    if(sargs_exists("cs"))
        return sargs_value("cs");
    return nullptr;
}

bool parse_milliseconds(char const* value, uint64_t& ms)
{
    if(value == nullptr || value[0] == '\0')
        return false;
    if(value[0] == '-')
        return false;

    errno = 0;
    char* end = nullptr;
    unsigned long long parsed = strtoull(value, &end, 10);
    if(errno != 0 || end == value || *end != '\0')
        return false;

    ms = (uint64_t)parsed;
    return true;
}

void attach_parent_console()
{
#ifdef _WIN32
    if(GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) != FILE_TYPE_UNKNOWN)
        return;
    if(!AttachConsole(ATTACH_PARENT_PROCESS))
        return;

    FILE* fp = nullptr;
    freopen_s(&fp, "CONOUT$", "wb", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    setvbuf(stdout, nullptr, _IONBF, 0);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

int clamp_int(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

bool capture_param(char const* name, char const* value)
{
    if(!name || !value)
        return false;

    int nvalue = atoi(value);

    if(!strcmp(name, "fxport"))
    {
        if(!strcmp(value, "d1") || !strcmp(value, "fx"))
            settings.fxport = FXPORT_D1;
        else if(!strcmp(value, "d2") || !strcmp(value, "fxdevkit"))
            settings.fxport = FXPORT_D2;
        else if(!strcmp(value, "32") || !strcmp(value, "mini"))
            settings.fxport = FXPORT_E2;
        else
            settings.fxport = clamp_int(nvalue, 0, FXPORT_NUM - 1);
        return true;
    }

    if(!strcmp(name, "display"))
    {
        if(!strcmp(value, "ssd1306"))
            settings.display = DISPLAY_SSD1306;
        else if(!strcmp(value, "ssd1309"))
            settings.display = DISPLAY_SSD1309;
        else if(!strcmp(value, "sh1106"))
            settings.display = DISPLAY_SH1106;
        else
            settings.display = clamp_int(nvalue, 0, DISPLAY_NUM - 1);
        return true;
    }

    return
        !strcmp(name, "size") ||
        !strcmp(name, "z") ||
        !strcmp(name, "g") ||
        !strcmp(name, "grid") ||
        !strcmp(name, "p") ||
        !strcmp(name, "palette") ||
        !strcmp(name, "af") ||
        !strcmp(name, "autofilter") ||
        !strcmp(name, "f") ||
        !strcmp(name, "filter") ||
        !strcmp(name, "ds") ||
        !strcmp(name, "downsample") ||
        !strcmp(name, "ori") ||
        !strcmp(name, "orientation") ||
        !strcmp(name, "v") ||
        !strcmp(name, "volume") ||
        !strcmp(name, "i") ||
        !strcmp(name, "intscale") ||
        !strcmp(name, "c") ||
        !strcmp(name, "current") ||
        !strcmp(name, "touch") ||
        !strcmp(name, "loading");
}

void apply_capture_settings()
{
    arduboy.cpu.data[0x23] = 0x10;
    arduboy.cpu.data[0x2c] = 0x40;
    arduboy.cpu.data[0x2f] = 0xf0;

    apply_emulation_settings(false);
}

bool load_capture_files()
{
    bool loaded_program = false;

    for(int i = 0; i < sargs_num_args(); ++i)
    {
        char const* key = sargs_key_at(i);
        char const* value = sargs_value_at(i);

        if(!strcmp(key, "captureserial") || !strcmp(key, "cs"))
            continue;
        if(capture_param(key, value))
            continue;

#if defined(ARDENS_DIST)
        continue;
#else
        std::ifstream f(value, std::ios::in | std::ios::binary);
        if(!f)
        {
            fprintf(stderr, "Could not open file: \"%s\"\n", value);
            return false;
        }

        bool save = !strcmp(key, "save");
        std::string err = arduboy.load_file(value, f, save);
        autoset_from_device_type();
        apply_capture_settings();
        if(!err.empty())
        {
            fprintf(stderr, "%s\n", err.c_str());
            return false;
        }
        if(!save && arduboy.cpu.decoded)
            loaded_program = true;
#endif
    }

#if defined(ARDENS_DIST)
    loaded_program = arduboy.cpu.decoded;
#endif

    if(!loaded_program)
    {
        fprintf(stderr, "No ROM file provided.\n");
        return false;
    }

    return true;
}

void write_serial_bytes()
{
    auto& bytes = arduboy.cpu.serial_bytes;
    if(!bytes.empty())
    {
        fwrite(bytes.data(), 1, bytes.size(), stdout);
        bytes.clear();
    }
}
}

bool capture_serial_requested()
{
    return capture_serial_value() != nullptr;
}

int capture_serial_run()
{
    attach_parent_console();

    uint64_t ms = 0;
    char const* value = capture_serial_value();
    if(!parse_milliseconds(value, ms))
    {
        fprintf(stderr, "Invalid captureserial value: \"%s\"\n", value ? value : "");
        return 1;
    }

    if(ms > std::numeric_limits<uint64_t>::max() / MS_PS)
    {
        fprintf(stderr, "captureserial value is too large.\n");
        return 1;
    }

    arduboy.display.type = absim::display_t::SSD1306;
    arduboy.fx.erase_all_data();
    arduboy.reset();
    arduboy.fx.min_page = 0xffff;
    arduboy.fx.max_page = 0xffff;
    apply_capture_settings();

    if(!load_capture_files())
        return 1;

    for(uint64_t i = 0; i < ms; ++i)
    {
        apply_capture_settings();
        arduboy.advance(MS_PS);
        write_serial_bytes();
        arduboy.cpu.sound_buffer.clear();
    }

    write_serial_bytes();
    fflush(stdout);
    return 0;
}

#endif
