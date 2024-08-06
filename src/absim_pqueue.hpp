#pragma once

#include <array>

#include "absim_config.hpp"

#include <stdint.h>

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
        cycles[least_index] = UINT64_MAX;
        update_next();
    }

    void clear()
    {
        for(auto& c : cycles)
            c = UINT64_MAX;
        least_cycle = UINT64_MAX;
        least_index = PQ_DUMMY;
    }

    template<class A> void serialize(A& a)
    {
        a(cycles, least_index, least_cycle);
    }

private:

    std::array<uint64_t, NUM_PQ> cycles;

    pqueue_type least_index;
    uint64_t least_cycle;

    void update_next()
    {
        least_cycle = UINT64_MAX;
        least_index = PQ_DUMMY;
        for(size_t i = 1; i < cycles.size(); ++i)
        {
            auto c = cycles[i];
            if(c < least_cycle)
            {
                least_cycle = c;
                least_index = (pqueue_type)i;
            }
        }
    }

};

}
