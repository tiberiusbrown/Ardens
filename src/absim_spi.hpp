#include "absim.hpp"

namespace absim
{

void atmega32u4_t::spi_handle_st_spcr_or_spsr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.update_spi();
    cpu.data[ptr] = x;
    uint8_t spcr = cpu.SPCR();
    uint8_t spsr = cpu.SPSR();
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

static uint8_t reverse_bits(uint8_t byte)
{
    static uint8_t const REVERSE[] =
    {
        0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
        0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
        0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
        0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
        0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
        0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
        0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
        0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
        0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
        0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
        0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
        0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
        0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
        0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
        0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
        0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
        0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
        0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
        0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
        0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
        0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
        0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
        0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
        0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
        0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
        0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
        0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
        0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
        0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
        0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
        0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
        0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
    };
    return REVERSE[byte];
}

void atmega32u4_t::spi_handle_st_spdr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.update_spi();
    uint8_t& spsr = cpu.SPSR();
    constexpr uint8_t SPIF = (1 << 7);
    constexpr uint8_t WCOL = (1 << 6);

    uint8_t spcr = cpu.SPCR();
    constexpr uint8_t DORD = (1 << 5);

    constexpr uint8_t SPE = (1 << 6);
    if(!(spcr & SPE))
        return;

    // Arduboy ain't no slave
    constexpr uint8_t MSTR = (1 << 4);
    if(!(spcr & MSTR) || cpu.spi_clock_cycles < 2)
        return;

    if(cpu.spi_busy)
    {
        spsr |= WCOL;
        cpu.autobreak(AB_SPI_WCOL);
    }
    else
    {
        if(cpu.spi_transmit_zero_cycle == cpu.cycle_count)
            cpu.spi_data_byte = 0;
        else
            cpu.spi_data_byte = spcr & DORD ? reverse_bits(x) : x;

        spsr &= ~(SPIF | WCOL);
        
        cpu.spi_done_cycle = cpu.cycle_count + cpu.spi_clock_cycles * 8;
        cpu.spi_transmit_zero_cycle = cpu.spi_done_cycle + 1;

        cpu.spi_busy = true;
    }
}

uint8_t atmega32u4_t::spi_handle_ld_spsr(atmega32u4_t& cpu, uint16_t ptr)
{
    cpu.update_spi();
    auto r = cpu.SPSR();
    if(!cpu.spi_busy && (r & 0x80))
        cpu.spsr_read_after_transmit = true;
    return r;
}

uint8_t atmega32u4_t::spi_handle_ld_spdr(atmega32u4_t& cpu, uint16_t ptr)
{
    cpu.update_spi();
    if(cpu.spsr_read_after_transmit)
    {
        cpu.spsr_read_after_transmit = false;
        cpu.SPSR() &= 0x3f;
    }
    assert(ptr == 0x4e);
    return cpu.SPDR();
}

ARDENS_FORCEINLINE void atmega32u4_t::update_spi()
{
    if(!spi_busy)
        return;

    uint8_t  spcr = SPCR();
    uint8_t& spsr = SPSR();

    constexpr uint8_t SPIE = (1 << 7);
    constexpr uint8_t SPE  = (1 << 6);
    constexpr uint8_t DORD = (1 << 5);
    constexpr uint8_t MSTR = (1 << 4);
    constexpr uint8_t CPOL = (1 << 3);
    constexpr uint8_t CPHA = (1 << 2);

    constexpr uint8_t SPIF = (1 << 7);
    constexpr uint8_t WCOL = (1 << 6);

    assert(spi_clock_cycles >= 2);

    if(!spi_latch_read && cycle_count >= spi_done_cycle)
    {
        spi_latch_read = true;
        spi_done_cycle += 1;
    }

    if(spi_latch_read && cycle_count >= spi_done_cycle)
    {
        SPDR() = spi_datain_byte;
        spi_busy = false;
        spi_latch_read = false;
        spi_data_latched = true;
        spsr_read_after_transmit = false;
        spsr |= SPIF;
        spi_done_cycle = UINT64_MAX;
    }
}

}
