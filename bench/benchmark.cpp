#include <benchmark/benchmark.h>

#include <absim.hpp>

#include <cstdlib>
#include <fstream>
#include <memory>


void SomeFunction() {}

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

    for (auto _ : state)
    {
        state.PauseTiming();
        arduboy->reset();
        state.ResumeTiming();
        arduboy->advance(100 * MS);
    }
}

BENCHMARK_CAPTURE(bench, ReturnOfTheArdu, "ReturnOfTheArdu.arduboy")
->Unit(benchmark::kMillisecond)
->Repetitions(10)
;

BENCHMARK_MAIN();
