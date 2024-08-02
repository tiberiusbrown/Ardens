#include <benchmark/benchmark.h>

#include <absim.hpp>

#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void save_screenshot(absim::arduboy_t const& a, std::string const& fname)
{
    stbi_write_png(fname.c_str(), 128, 64, 1, a.display.filtered_pixels.data(), 128 * 1);
}

static void bench(benchmark::State& state, std::string const& fname)
{
    constexpr uint64_t MS = 1'000'000'000ull;

    auto arduboy = std::make_unique<absim::arduboy_t>();
    {
        std::string path = std::string(ARDENS_BENCHMARK_DIR) + "/" + fname;
        std::ifstream f(path, std::ios::binary);
        if(!f.good())
            exit(1);
        arduboy->load_file(path.c_str(), f);
    }

    std::stringstream ss;
    arduboy->fxport_reg = 0x2b;
    arduboy->fxport_mask = 1 << 1;
    arduboy->display.enable_filter = true;
    arduboy->advance(1000 * MS);
    arduboy->save_savestate(ss);

    uint64_t c0, c1;
    for (auto _ : state)
    {
        state.PauseTiming();
        arduboy->reset();
        ss.seekg(0);
        if("" != arduboy->load_savestate(ss))
            break;
        arduboy->fxport_reg = 0x2b;
        arduboy->fxport_mask = 1 << 1;
        arduboy->display.enable_filter = true;
        state.ResumeTiming();
        arduboy->advance(100 * MS);
    }

    save_screenshot(*arduboy, fname + ".png");
}

BENCHMARK_CAPTURE(bench, ReturnOfTheArdu, "ReturnOfTheArdu.arduboy")
->Unit(benchmark::kMillisecond)
->Repetitions(1)
;

BENCHMARK_CAPTURE(bench, racing_game, "racing_game.hex")
->Unit(benchmark::kMillisecond)
->Repetitions(1)
;

BENCHMARK_MAIN();
