#include "common.hpp"

#ifdef __EMSCRIPTEN__

void file_watch(std::string const& filename) { (void)filename; }
void file_watch_clear() {}
void file_watch_check() {}

#else

#include <atomic>
#include <fstream>

#include <filewatch/FileWatch.hpp>

using Watch = filewatch::FileWatch<std::string>;

static std::unique_ptr<Watch> watch_hex;
static std::unique_ptr<Watch> watch_bin;

static std::string fname_hex;
static std::string fname_bin;

std::atomic<bool> load_hex;
std::atomic<bool> load_bin;

template<bool hex>
static void watch_action(std::string const& path, filewatch::Event const e)
{
    if(e != filewatch::Event::modified) return;
    if(hex)
        load_hex.exchange(true);
    else
        load_bin.exchange(true);
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
    load_hex.store(false);
    load_bin.store(false);
    watch_hex.reset();
    watch_bin.reset();
}

void file_watch_check()
{
    if(load_hex.exchange(false) && arduboy)
    {
        std::ifstream f(fname_hex.c_str(), std::ios::in | std::ios::binary);
        if(f.fail()) load_hex.store(true);
        else dropfile_err = arduboy->load_file(fname_hex.c_str(), f);
    }
    if(load_bin.exchange(false) && arduboy)
    {
        std::ifstream f(fname_bin.c_str(), std::ios::in | std::ios::binary);
        if(f.fail()) load_bin.store(true);
        else dropfile_err = arduboy->load_file(fname_bin.c_str(), f);
    }
}

#endif
