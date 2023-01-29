#include "absim.hpp"

namespace absim
{

void FORCEINLINE atmega32u4_t::cycle_pll(uint32_t cycles)
{
    uint8_t& csr = data[0x49];
    uint8_t frq = data[0x52];

    constexpr uint8_t PLOCK = 1 << 0;
    constexpr uint8_t PLLE = 1 << 1;

    if(!(csr & PLLE))
    {
        csr &= ~PLOCK;
        pll_lock_cycle = 0;
        return;
    }

    if(csr & PLOCK)
        return;

    // assume PLL takes 3 ms to lock
    constexpr uint64_t LOCK_CYCLES = 16000000 * 3 / 1000;

    if((pll_lock_cycle += cycles) >= LOCK_CYCLES)
        csr |= PLOCK;
}

}
