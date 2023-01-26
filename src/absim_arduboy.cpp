#include "absim.hpp"

namespace absim
{

// each cycle is 62.5 ns
constexpr uint64_t CYCLE_PS = 62500;

void arduboy_t::cycle()
{
    cpu.advance_cycle();

    // TODO: model SPI connection more precisely?
    // send SPI commands and data to display
    if(cpu.spi_done)
    {
        uint8_t byte = cpu.spi_data_byte;
        uint8_t portd = cpu.data[0x2b];
        if(!(portd & (1 << 6)))
        {
            if(portd & (1 << 4))
                display.send_data(byte);
            else
                display.send_command(byte);
        }
    }

    // each cycle is 62.5 ns
    constexpr uint64_t dps = 62500;

    display.advance(CYCLE_PS);
}

void arduboy_t::advance_instr()
{
    int n = 0;
    do
    {
        cpu.advance_cycle();
    } while(++n < 65536 && (!cpu.active || cpu.cycles_till_next_instr != 0));
}

void arduboy_t::advance(uint64_t ps)
{
    ps += ps_rem;
    ps_rem = 0;

    if(paused) return;

    while(ps >= CYCLE_PS)
    {
        cycle();
        
        ps -= CYCLE_PS;

        if(cpu.pc < cpu.breakpoints.size() &&
            cpu.breakpoints.test(cpu.pc))
        {
            paused = true;
            return;
        }
    }

    // track remainder
    ps_rem = ps;
}

}
