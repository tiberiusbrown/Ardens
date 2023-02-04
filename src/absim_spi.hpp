#include "absim.hpp"

namespace absim
{

FORCEINLINE void atmega32u4_t::cycle_spi(uint32_t cycles)
{
    uint8_t  spcr = data[0x4c];
    uint8_t& spsr = data[0x4d];

    // cycles per SPI clock
    //if(just_written == 0x4c || just_written == 0x4d)
    if((just_written >> 1) == (0x4c >> 1))
    {
        uint8_t n = ((spsr & 0x1) << 2) | (spcr & 0x3);
        switch(n)
        {
        case 0: spi_clock_cycles = 4;   break;
        case 1: spi_clock_cycles = 16;  break;
        case 2: spi_clock_cycles = 64;  break;
        case 3: spi_clock_cycles = 128; break;
        case 4: spi_clock_cycles = 2;   break;
        case 5: spi_clock_cycles = 8;   break;
        case 6: spi_clock_cycles = 32;  break;
        case 7: spi_clock_cycles = 64;  break;
        default: break;
        }
    }

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

    assert(spi_clock_cycles >= 2);

    uint32_t iters = 0;

    spi_clock_cycle += cycles;
    while(spi_clock_cycle >= spi_clock_cycles)
        ++iters, spi_clock_cycle -= spi_clock_cycles;
    
    uint8_t spdr = data[0x4e];
    while(iters-- != 0)
    {
        if(spcr & DORD)
        {
            uint8_t b = spdr & 0x1;
            spdr >>= 1;
            spdr |= (spi_datain_byte & 0x80);
            spi_datain_byte <<= 1;
            spi_data_byte = (spi_data_byte << 1) | b;
        }
        else
        {
            uint8_t b = spdr >> 7;
            spdr <<= 1;
            spdr |= (spi_datain_byte >> 7);
            spi_datain_byte <<= 1;
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
    data[0x4e] = spdr;
}

}
