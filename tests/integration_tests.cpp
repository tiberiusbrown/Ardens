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
    auto const& d = arduboy->cpu.serial_bytes;
    if(err.empty())
    {
        for(int i = 0; i < 10000; ++i) // up to ten seconds
        {
            arduboy->advance(1'000'000'000ull); // 1 ms
            if(!d.empty()) break;
        }
    }

    bool pass = (d.size() == 1 && d[0] == 'P');

    printf("%-30s : %s\n", name, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int main()
{
    int r = 0;
    arduboy = std::make_unique<absim::arduboy_t>();

    r |= test("float");
    r |= test("instructions");
    r |= test("signature");
    r |= test("timer_tcnt_write");

    return r;
}
