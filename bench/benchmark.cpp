#include <benchmark/benchmark.h>

#include <absim.hpp>

#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static absim::arduboy_t arduboy;

void save_screenshot(absim::arduboy_t const& a, std::string const& fname)
{
    stbi_write_png(fname.c_str(), 128, 64, 1, a.display.filtered_pixels.data(), 128 * 1);
}

static void bench(benchmark::State& state, std::string const& fname)
{
    constexpr uint64_t MS = 1'000'000'000ull;

    //auto arduboy = std::make_unique<absim::arduboy_t>();
    {
        std::string path = std::string(ARDENS_BENCHMARK_DIR) + "/" + fname;
        std::ifstream f(path, std::ios::binary);
        if(!f.good())
            exit(1);
        if("" != arduboy.load_file(path.c_str(), f))
            return;
    }

    std::stringstream ss;
    uint8_t pinf = 0xf0;
    uint8_t pine = 0x40;
    uint8_t pinb = 0x10;

    arduboy.fxport_reg = 0x2b;
    arduboy.fxport_mask = 1 << 1;
    arduboy.display.enable_filter = true;
    arduboy.cpu.data[0x23] = pinb;
    arduboy.cpu.data[0x2c] = pine;
    arduboy.cpu.data[0x2f] = pinf;
    arduboy.advance(2000 * MS);
    arduboy.save_savestate(ss);
    save_screenshot(arduboy, fname + ".pre.png");

    for(auto _ : state)
    {
        state.PauseTiming();
        ss.seekg(0);
        if("" != arduboy.load_savestate(ss))
            break;
        arduboy.fxport_reg = 0x2b;
        arduboy.fxport_mask = 1 << 1;
        arduboy.display.enable_filter = true;
        arduboy.cpu.data[0x23] = pinb;
        arduboy.cpu.data[0x2c] = pine;
        arduboy.cpu.data[0x2f] = pinf;
        state.ResumeTiming();
        arduboy.advance(100 * MS);
    }

    save_screenshot(arduboy, fname + ".post.png");
}

BENCHMARK_CAPTURE(bench, ReturnOfTheArdu, "ReturnOfTheArdu.arduboy")
->Unit(benchmark::kMillisecond)
->MinTime(10.0)
;

//BENCHMARK_CAPTURE(bench, racing_game, "racing_game.hex")
//->Unit(benchmark::kMillisecond)
//->MinTime(3.0)
//;
//
//BENCHMARK_CAPTURE(bench, ardugolf, "ardugolf.hex")
//->Unit(benchmark::kMillisecond)
//->MinTime(3.0)
//;

BENCHMARK_MAIN();
