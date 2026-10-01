#include "common.hpp"

#ifdef __EMSCRIPTEN__

void file_watch(std::string const& filename) { (void)filename; }
void file_watch_clear() {}
void file_watch_check() {}

#else

#include <chrono>
#include <fstream>
#include <mutex>

#include <filewatch/FileWatch.hpp>

constexpr uint64_t RELOAD_AFTER_MS = 500;

using Watch = filewatch::FileWatch<std::string>;

static uint64_t ms_reload_hex = 0;
static uint64_t ms_reload_bin = 0;
static std::mutex reload_mutex;
static bool load_hex = false;
static bool load_bin = false;

static std::unique_ptr<Watch> watch_hex;
static std::unique_ptr<Watch> watch_bin;

static std::string fname_hex;
static std::string fname_bin;

static uint64_t monotonic_ms()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

template<bool hex>
static void watch_action(std::string const& path, filewatch::Event const e)
{
    if(e != filewatch::Event::modified &&
        e != filewatch::Event::added &&
        e != filewatch::Event::renamed_new) return;
    std::lock_guard<std::mutex> lock(reload_mutex);
    if(hex)
    {
        ms_reload_hex = monotonic_ms() + RELOAD_AFTER_MS;
        load_hex = true;
    }
    else
    {
        ms_reload_bin = monotonic_ms() + RELOAD_AFTER_MS;
        load_bin = true;
    }
}

void file_watch(std::string const& filename)
{
    if(ends_with(filename, ".hex")
#ifdef ARDENS_LLVM
        || ends_with(filename, ".elf")
#endif
        )
    {
        fname_hex = filename;
        watch_hex = std::make_unique<Watch>(filename, watch_action<true>);
    }
    if(ends_with(filename, ".bin"))
    {
        fname_bin = filename;
        watch_bin = std::make_unique<Watch>(filename, watch_action<false>);
    }
#ifndef ARDENS_NO_ARDUBOY_FILE
    if(ends_with(filename, ".arduboy"))
    {
        fname_hex = filename;
        watch_hex = std::make_unique<Watch>(filename, watch_action<true>);
        watch_bin.reset();
    }
#endif
}

void file_watch_clear()
{
    watch_hex.reset();
    watch_bin.reset();
    std::lock_guard<std::mutex> lock(reload_mutex);
    load_hex = false;
    load_bin = false;
}

void file_watch_check()
{
    const uint64_t now = monotonic_ms();
    bool reload_hex = false;
    bool reload_bin = false;
    {
        std::lock_guard<std::mutex> lock(reload_mutex);
        if(load_hex && now >= ms_reload_hex)
        {
            load_hex = false;
            reload_hex = true;
        }
        if(load_bin && now >= ms_reload_bin)
        {
            load_bin = false;
            reload_bin = true;
        }
    }
    if(reload_hex)
    {
        std::ifstream f(fname_hex.c_str(), std::ios::in | std::ios::binary);
        if(f.fail())
        {
            std::lock_guard<std::mutex> lock(reload_mutex);
            if(!load_hex)
            {
                load_hex = true;
                ms_reload_hex = monotonic_ms();
            }
        }
        else
        {
            disconnect_linked_secondary_arduboy();
            app.dropfile_err = app.emulator->load_file(fname_hex.c_str(), f);
        }
    }
    if(reload_bin)
    {
        std::ifstream f(fname_bin.c_str(), std::ios::in | std::ios::binary);
        if(f.fail())
        {
            std::lock_guard<std::mutex> lock(reload_mutex);
            if(!load_bin)
            {
                load_bin = true;
                ms_reload_bin = monotonic_ms();
            }
        }
        else
        {
            disconnect_linked_secondary_arduboy();
            app.dropfile_err = app.emulator->load_file(fname_bin.c_str(), f);
        }
    }
}

#endif
