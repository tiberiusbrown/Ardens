#pragma once

#include <array>

#include "absim_config.hpp"

#include <stdint.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace absim
{

enum pqueue_type
{
    PQ_DUMMY,
    PQ_SPI,
    PQ_TIMER0,
    PQ_TIMER1,
    PQ_TIMER3,
    PQ_TIMER4,
    PQ_USB,
    PQ_WATCHDOG,
    PQ_EEPROM,
    PQ_PLL,
    PQ_ADC,
    PQ_SPM,
    PQ_INTERRUPT,
    NUM_PQ
};

struct pqueue_item
{
    uint64_t cycle;
    pqueue_type type;
};

// A priority queue for scheduling peripheral update cycles
struct pqueue
{
    ARDENS_FORCEINLINE void schedule(uint64_t cycle, pqueue_type type)
    {
        if(cycles[type] > cycle)
        {
            cycles[type] = cycle;
            scheduled |= (1 << type);
            if(cycle < least_cycle)
            {
                least_cycle = cycle;
                least_index = type;
            }
        }
    }

    ARDENS_FORCEINLINE pqueue_item next() const
    {
        return { least_cycle, least_index };
    }

    ARDENS_FORCEINLINE uint64_t next_cycle() const
    {
        return least_cycle;
    }

    ARDENS_FORCEINLINE void pop()
    {
        scheduled &= ~(1 << least_index);
        cycles[least_index] = UINT64_MAX;
        update_least();
    }

    void clear()
    {
        for(auto& c : cycles)
            c = UINT64_MAX;
        least_cycle = UINT64_MAX;
        least_index = PQ_DUMMY;
        scheduled = 0;
    }

    template<class A> void serialize(A& a)
    {
        a(cycles, least_index, least_cycle, scheduled);
    }

private:

    std::array<uint64_t, NUM_PQ> cycles;

    pqueue_type least_index;
    uint64_t least_cycle;

    uint32_t scheduled; // bitset

    ARDENS_FORCEINLINE void update_least()
    {
#if 1
        uint32_t i = 0;
        uint64_t c = UINT64_MAX;
        uint32_t s = scheduled;
        while(s != 0)
        {
            uint32_t t = s & uint32_t(-(int32_t)s);
#if defined(__GNUC__) || defined(__clang__)
            int r = (unsigned)__builtin_ctz(s);
#elif defined(_MSC_VER)
            unsigned long r;
            (void)_BitScanForward(&r, s);
#else
            int r = 0;
            while(!(s & 1))
                ++r, s >>= 1;
#endif
            uint64_t tc = cycles[r];
            if(tc < c)
            {
                c = tc;
                i = (uint32_t)r;
            }
            s ^= t;
        }
        least_index = (pqueue_type)i;
        least_cycle = c;
#else
        least_cycle = cycles[1];
        least_index = (pqueue_type)1;
        for(size_t i = 2; i < cycles.size(); ++i)
        {
            auto c = cycles[i];
            if(c < least_cycle)
            {
                least_cycle = c;
                least_index = (pqueue_type)i;
            }
        }
#endif
    }

};

}
