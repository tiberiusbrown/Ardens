#include <absim.hpp>

#include <cinttypes>
#include <cstdio>
#include <cstdlib>

#include <fstream>

static absim::arduboy_t arduboy;

int main(int argc, char** argv)
{
    if(argc < 2)
        exit(1);

    {
        std::ifstream f(argv[1], std::ios::binary);
        if(!f.good())
            exit(1);
        if("" != arduboy.load_file(argv[1], f))
            exit(1);
    }

    if(argc >= 3)
    {
        std::ifstream f(argv[2], std::ios::binary);
        if(!f.good())
            exit(1);
        if("" != arduboy.load_file(argv[2], f))
            exit(1);
    }

    uint8_t pinf = 0xf0;
    uint8_t pine = 0x40;
    uint8_t pinb = 0x10;
    arduboy.cpu.data[0x23] = pinb;
    arduboy.cpu.data[0x2c] = pine;
    arduboy.cpu.data[0x2f] = pinf;
    arduboy.fxport_reg = 0x2b;
    arduboy.fxport_mask = 1 << 1;
    arduboy.profiler_enabled = true;
    arduboy.cpu.enabled_autobreaks.set();

    constexpr uint64_t MS = 1'000'000'000ull;

    uint64_t prev_cycle = 0;

    while(arduboy.cpu.active)
    {
        if(arduboy.paused)
        {
            if(prev_cycle == 0)
            {
                prev_cycle = arduboy.cpu.cycle_count;
            }
            else
            {
                prev_cycle = arduboy.cpu.cycle_count - prev_cycle - 1;
                printf(" %" PRIu64 " |", prev_cycle);
                prev_cycle = 0;
            }
        }

        arduboy.paused = false;
        arduboy.advance(1 * MS);

        if(!arduboy.cpu.serial_bytes.empty())
        {
            for(auto b : arduboy.cpu.serial_bytes)
                printf("%c", (char)b);
            arduboy.cpu.serial_bytes.clear();
        }

        // quit after ten seconds simulated
        if(arduboy.cpu.cycle_count >= 10000 * MS)
            break;
    }

    return 0;
}
