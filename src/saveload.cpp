#include "common.hpp"

#include <fstream>

#include <fmt/format.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static std::string savedata_filename()
{
    return fmt::format(
#ifdef __EMSCRIPTEN__
        "/offline/"
#endif
        "absim_{:#016x}.save", arduboy->game_hash);
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
