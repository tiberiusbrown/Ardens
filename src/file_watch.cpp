#include "common.hpp"

#ifdef __EMSCRIPTEN__

void file_watch(std::string const& filename) { (void)filename; }
void file_watch_clear() {}

#else

#include <fstream>

#include <filewatch/FileWatch.hpp>

using Watch = filewatch::FileWatch<std::string>;

static std::unique_ptr<Watch> watch_hex;
static std::unique_ptr<Watch> watch_bin;

static std::string fname_hex;
static std::string fname_bin;

template<bool hex>
static void watch_action(std::string const& path, filewatch::Event const e)
{
    if(e != filewatch::Event::modified) return;
    if(!arduboy) return;
    std::string fname = hex ? fname_hex : fname_bin;
    std::ifstream f(fname.c_str(), std::ios::in | std::ios::binary);
    if(f.fail()) return;
    dropfile_err = arduboy->load_file(fname.c_str(), f);
}

void file_watch(std::string const& filename)
{
    if(ends_with(filename, ".hex"))
    {
        fname_hex = filename;
        watch_hex = std::make_unique<Watch>(filename, watch_action<true>);
    }
    if(ends_with(filename, ".bin"))
    {
        fname_bin = filename;
        watch_bin = std::make_unique<Watch>(filename, watch_action<false>);
    }
    if(ends_with(filename, ".arduboy"))
    {
        fname_hex = filename;
        watch_hex = std::make_unique<Watch>(filename, watch_action<true>);
        watch_bin.reset();
    }
}

void file_watch_clear()
{
    watch_hex.reset();
    watch_bin.reset();
}

#endif
