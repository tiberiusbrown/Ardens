#include "absim.hpp"

namespace absim
{

void atmega32u4_t::pll_handle_st_pllcsr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == 0x49);
    constexpr uint8_t PLOCK = 1 << 0;
    constexpr uint8_t PLLE = 1 << 1;
    uint8_t& csr = cpu.data[0x49];
    if(!(x & PLLE))
    {
        x &= ~PLOCK;
        cpu.pll_lock_cycle = 0;
        cpu.pll_busy = false;
    }
    else
    {
        cpu.pll_busy = true;
    }

    // PLOCK is read only
    x &= ~PLOCK;
    x |= (csr & PLOCK);

    csr = x;
}


void ABSIM_FORCEINLINE atmega32u4_t::cycle_pll(uint32_t cycles)
{
    if(!pll_busy)
        return;

    uint8_t& csr = data[0x49];
    //uint8_t frq = data[0x52];

    constexpr uint8_t PLOCK = 1 << 0;

    // assume PLL takes 3 ms to lock
    constexpr uint64_t LOCK_CYCLES = 16000000 * 3 / 1000;

    if((pll_lock_cycle += cycles) >= LOCK_CYCLES)
    {
        csr |= PLOCK;
        pll_busy = false;
    }
}

}
