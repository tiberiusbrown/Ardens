#include "common.hpp"

#include <fstream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <inttypes.h>

constexpr uint64_t SAVE_INTERVAL_MS = 500;

static bool need_save = false;
static uint64_t need_save_time;

std::string savedata_filename()
{
    char buf[128];
    snprintf(buf, sizeof(buf), "absim_%" PRIx64 ".save", app.emulator->program_state.game_hash);
    return (app.userpath / buf).generic_string();
}

void load_savedata()
{
    auto fname = savedata_filename();
    std::ifstream f(fname, std::ios::in | std::ios::binary);
    if(!f.fail())
    {
        printf("Loaded %s\n", fname.c_str());
        app.emulator->load_savedata(f);
    }
}

void check_save_savedata()
{
    if(app.emulator->save_data_state.dirty)
    {
        need_save = true;
        need_save_time = app.ms_since_start + SAVE_INTERVAL_MS;
        app.emulator->save_data_state.dirty = false;
    }

    if(need_save && app.ms_since_start >= need_save_time)
    {
        flush_savedata();
    }
}

void flush_savedata()
{
    if(!need_save && !app.emulator->save_data_state.dirty)
    {
        return;
    }

    need_save = false;
    app.emulator->save_data_state.dirty = false;

    auto fname = savedata_filename();
    std::ofstream f(fname, std::ios::out | std::ios::binary);
    if(!f.fail())
    {
        app.emulator->save_savedata(f);
        f.close();
#ifdef __EMSCRIPTEN__
        EM_ASM(
            FS.syncfs(function(err) {});
        );
#endif
        printf("Saved %s\n", fname.c_str());
    }
}
