#include <absim.hpp>

#include <cstdio>
#include <fstream>
#include <memory>

static std::unique_ptr<absim::arduboy_t> arduboy;

static int test(char const* name)
{
    int r = 0;

    std::string fname =
        std::string(TESTS_DIR "/") + name + "/" + name + ".ino-arduboy-fx.hex";
    std::ifstream f(fname);

    auto err = arduboy->load_file("test.hex", f);
    arduboy->reset();
    if(err.empty())
        arduboy->advance(100'000'000'000ull); // 100 ms

    auto const& d = arduboy->cpu.serial_bytes;
    bool pass = (d.size() == 1 && d[0] == 'P');

    printf("%-30s : %s\n", name, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int main()
{
    int r = 0;
    arduboy = std::make_unique<absim::arduboy_t>();

    r |= test("timer_tcnt_write");
    r |= test("timer_prescaler_reset");
    r |= test("timer_prescaler_reset2");

    return r;
}
