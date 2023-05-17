#include "common.hpp"

#include <fstream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <inttypes.h>

constexpr uint64_t SAVE_INTERVAL_MS = 500;

static bool need_save = false;
static uint64_t need_save_time;

static std::string savedata_filename()
{
    char buf[128];
    snprintf(buf, sizeof(buf),
#ifdef __EMSCRIPTEN__
        "/offline/"
#endif
        "absim_%" PRIx64 ".save", arduboy->game_hash);
    return std::string(buf);
}

void load_savedata()
{
    auto fname = savedata_filename();
    std::ifstream f(fname, std::ios::in | std::ios::binary);
    if(!f.fail())
    {
        printf("Loaded %s\n", fname.c_str());
        arduboy->load_savedata(f);
    }
}

void check_save_savedata()
{
    if(arduboy->savedata_dirty)
    {
        need_save = true;
        need_save_time = ms_since_start + SAVE_INTERVAL_MS;
        arduboy->savedata_dirty = false;
    }

    if(need_save && ms_since_start >= need_save_time)
    {
        need_save = false;
        auto fname = savedata_filename();
        std::ofstream f(fname, std::ios::out | std::ios::binary);
        if(!f.fail())
        {
            arduboy->save_savedata(f);
            f.close();
#ifdef __EMSCRIPTEN__
            EM_ASM(
                FS.syncfs(function(err) {});
            );
#endif
        }
    }
}
