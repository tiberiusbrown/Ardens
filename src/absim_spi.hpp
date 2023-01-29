#include "absim.hpp"

namespace absim
{

void FORCEINLINE atmega32u4_t::cycle_spi(uint32_t cycles)
{
    uint8_t  spcr = data[0x4c];
    uint8_t& spsr = data[0x4d];
    uint8_t& spdr = data[0x4e];

    constexpr uint8_t SPIE = (1 << 7);
    constexpr uint8_t SPE  = (1 << 6);
    constexpr uint8_t DORD = (1 << 5);
    constexpr uint8_t MSTR = (1 << 4);
    constexpr uint8_t CPOL = (1 << 3);
    constexpr uint8_t CPHA = (1 << 2);

    constexpr uint8_t SPIF = (1 << 7);
    constexpr uint8_t WCOL = (1 << 6);

    if(!(spcr & SPE))
    {
        spi_busy = false;
        return;
    }
    
    if(just_written == 0x4e)
    {
        if(spi_busy)
            spsr |= WCOL;
        else
        {
            if(spsr_read_after_transmit)
                spsr &= ~(SPIF | WCOL);
            spi_data_byte = 0;
            spi_clock_cycle = 0;
            spi_bit_progress = 0;
            spi_busy = true;
        }
    }
    else if(just_read == 0x4d)
        spsr_read_after_transmit = true;

    if(!spi_busy)
        return;

    // Arduboy ain't no slave
    if(!(spcr & MSTR))
        return;

    // cycles per SPI clock
    uint8_t clock_cycles = 0;
    {
        uint8_t n = ((spsr & 0x1) << 2) | (spcr & 0x3);
        switch(n)
        {
        case 0: clock_cycles = 4;   break;
        case 1: clock_cycles = 16;  break;
        case 2: clock_cycles = 64;  break;
        case 3: clock_cycles = 128; break;
        case 4: clock_cycles = 2;   break;
        case 5: clock_cycles = 8;   break;
        case 6: clock_cycles = 32;  break;
        case 7: clock_cycles = 64;  break;
        default: break;
        }
        if(clock_cycles == 0)
            return;
    }

    uint32_t iters = 0;

    spi_clock_cycle += cycles;
    while(spi_clock_cycle >= clock_cycles)
        ++iters, spi_clock_cycle -= clock_cycles;
    
    while(iters-- != 0)
    {
        if(spcr & DORD)
        {
            uint8_t b = spdr & 0x1;
            spdr >>= 1;
            spi_data_byte = (spi_data_byte << 1) | b;
        }
        else
        {
            uint8_t b = spdr >> 7;
            spdr <<= 1;
            spi_data_byte = (spi_data_byte << 1) | b;
        }

        ++spi_bit_progress;

        if(spi_bit_progress == 8)
        {
            spi_done = true;
            spi_busy = false;
            spsr |= SPIF;
            break;
        }
    }
}

}
