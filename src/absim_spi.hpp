#include "absim.hpp"

namespace absim
{

void atmega32u4_t::spi_handle_st_spcr_or_spsr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr] = x;
    uint8_t spcr = cpu.data[0x4c];
    uint8_t spsr = cpu.data[0x4d];
    uint8_t n = ((spsr & 0x1) << 2) | (spcr & 0x3);
    switch(n)
    {
    case 0: cpu.spi_clock_cycles = 4;   break;
    case 1: cpu.spi_clock_cycles = 16;  break;
    case 2: cpu.spi_clock_cycles = 64;  break;
    case 3: cpu.spi_clock_cycles = 128; break;
    case 4: cpu.spi_clock_cycles = 2;   break;
    case 5: cpu.spi_clock_cycles = 8;   break;
    case 6: cpu.spi_clock_cycles = 32;  break;
    case 7: cpu.spi_clock_cycles = 64;  break;
    default: break;
    }
    constexpr uint8_t SPE = (1 << 6);
    if(!(spcr & SPE))
        cpu.spi_busy = false;

    // Arduboy ain't no slave
    constexpr uint8_t MSTR = (1 << 4);
    if(!(spcr & MSTR))
        cpu.spi_busy = false;
}

void atmega32u4_t::spi_handle_st_spdr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    uint8_t& spsr = cpu.data[0x4d];
    constexpr uint8_t SPIF = (1 << 7);
    constexpr uint8_t WCOL = (1 << 6);

    cpu.data[ptr] = x;

    uint8_t spcr = cpu.data[0x4c];

    constexpr uint8_t SPE = (1 << 6);
    if(!(spcr & SPE))
        return;

    // Arduboy ain't no slave
    constexpr uint8_t MSTR = (1 << 4);
    if(!(spcr & MSTR))
        return;

    if(cpu.spi_busy)
    {
        spsr |= WCOL;
        cpu.autobreak(AB_SPI_WCOL);
    }
    else
    {
        //if(cpu.spsr_read_after_transmit)
        //{
        //    cpu.spsr_read_after_transmit = false;
        //    spsr &= ~(SPIF | WCOL);
        //}
        spsr &= ~(SPIF | WCOL);
        cpu.spi_data_byte = 0;
        cpu.spi_clock_cycle = 0;
        cpu.spi_bit_progress = 0;
        cpu.spi_busy = true;
    }
}

uint8_t atmega32u4_t::spi_handle_ld_spsr(atmega32u4_t& cpu, uint16_t ptr)
{
    auto r = cpu.data[0x4d];
    if(!cpu.spi_busy && (r & 0x80))
        cpu.spsr_read_after_transmit = true;
    return r;
}

uint8_t atmega32u4_t::spi_handle_ld_spdr(atmega32u4_t& cpu, uint16_t ptr)
{
    if(cpu.spsr_read_after_transmit)
    {
        cpu.spsr_read_after_transmit = false;
        cpu.data[0x4d] &= 0x3f;
    }
    assert(ptr == 0x4e);
    return cpu.data[0x4e];
}

ABSIM_FORCEINLINE void atmega32u4_t::cycle_spi(uint32_t cycles)
{
    if(!spi_busy)
        return;

    uint8_t  spcr = data[0x4c];
    uint8_t& spsr = data[0x4d];

    constexpr uint8_t SPIE = (1 << 7);
    constexpr uint8_t SPE  = (1 << 6);
    constexpr uint8_t DORD = (1 << 5);
    constexpr uint8_t MSTR = (1 << 4);
    constexpr uint8_t CPOL = (1 << 3);
    constexpr uint8_t CPHA = (1 << 2);

    constexpr uint8_t SPIF = (1 << 7);
    constexpr uint8_t WCOL = (1 << 6);

    assert(spi_clock_cycles >= 2);

    uint32_t iters = 0;

    auto cc = spi_clock_cycles / 2;
    spi_clock_cycle += cycles;
    while(spi_clock_cycle >= cc)
        ++iters, spi_clock_cycle -= cc;
    
    uint8_t spdr = data[0x4e];
    while(iters-- != 0)
    {

        bool clock_hi = (spi_bit_progress & 1) != 0;

        ++spi_bit_progress;

        if(clock_hi && spi_bit_progress <= 16)
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

            if(spi_bit_progress >= 15)
                spi_done_shifting = true;
        }

        if(spi_bit_progress == 17)
        {
            spsr_read_after_transmit = false;
            spi_done = true;
            spi_busy = false;
            spsr |= SPIF;
            break;
        }
    }
    data[0x4e] = spdr;
}

}
