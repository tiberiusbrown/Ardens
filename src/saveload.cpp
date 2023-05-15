#include "common.hpp"

#include <fstream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <inttypes.h>

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
    if(!arduboy->savedata_dirty) return;
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
