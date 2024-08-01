#include "absim.hpp"

namespace absim
{

// assume PLL takes 3 ms to lock
constexpr uint64_t PLL_LOCK_CYCLES = 16000000 * 3 / 1000;

void atmega32u4_t::pll_handle_st_pllcsr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == 0x49);
    constexpr uint8_t PLOCK = 1 << 0;
    constexpr uint8_t PLLE = 1 << 1;

    cpu.pll_num12 = 0;
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

    uint8_t& csr = cpu.data[0x49];

    // PLOCK is read only
    x &= ~PLOCK;
    x |= (csr & PLOCK);

    csr = x;

    cpu.pll_schedule();
}

ARDENS_FORCEINLINE void atmega32u4_t::pll_schedule()
{
    if(pll_busy)
    {
        peripheral_queue.schedule(
            cycle_count + (PLL_LOCK_CYCLES - pll_lock_cycle), PQ_PLL);
    }
}

ARDENS_FORCEINLINE void atmega32u4_t::update_pll()
{
    if(!pll_busy)
        return;

    uint32_t cycles = uint32_t(cycle_count - pll_prev_cycle);
    pll_prev_cycle = cycle_count;

    uint8_t& csr = data[0x49];

    constexpr uint8_t PLOCK = 1 << 0;

    if((pll_lock_cycle += cycles) >= PLL_LOCK_CYCLES)
    {
        csr |= PLOCK;
        pll_busy = false;

        // compute pll numerator
        uint32_t frq = data[0x52];

        uint32_t divider = frq & 0xf;

        uint32_t f = 0;
        if(divider >= 3 && divider <= 10 && divider != 6)
            f = divider * 8 + 16;
        uint32_t tm = (frq >> 4) & 0x3;
        pll_num12 = 0;
        if(tm == 1) // D = 1
            pll_num12 = f / 4 * 3;
        else if(tm == 2) // D = 1.5
            pll_num12 = f / 2;
        else if(tm == 3) // D = 2
            pll_num12 = f / 8 * 3;

        update_timer4();
    }

    pll_schedule();
}

}
