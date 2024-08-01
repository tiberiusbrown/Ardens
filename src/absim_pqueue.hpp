#pragma once

#include <queue>
#include <vector>

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
    NUM_PQ
};

struct pqueue_item
{
    uint64_t cycle;
    pqueue_type type;
};

struct pqueue_compare
{
    bool operator()(pqueue_item a, pqueue_item b)
    {
        return a.cycle > b.cycle;
    }
};

// A priority queue for scheduling peripheral update cycles
struct pqueue
{
    void schedule(uint64_t cycle, pqueue_type type);

    pqueue_item next() const
    {
        return q.top();
    }

    uint64_t next_cycle() const
    {
        return next().cycle;
    }

    void pop()
    {
        pqueue_type t = q.top().type;
        if(t == PQ_DUMMY) return;
        scheduled[t] = false;
        q.pop();
    }

    void clear_to_cycle(uint64_t cycle)
    {
        for(;;)
        {
            auto t = q.top();
            if(t.cycle >= cycle) break;
            scheduled[t.type] = false;
            q.pop();
        }
    }

    void clear()
    {
        q.swap(decltype(q){});
        q.push({ UINT64_MAX, PQ_DUMMY });
        scheduled = {};
    }

private:

    struct queue_type : public std::priority_queue<
        pqueue_item,
        std::vector<pqueue_item>,
        pqueue_compare>
    {
        auto& container() { return c; }
        auto comparator() { return comp; }
    };

    queue_type q;

    std::array<bool, NUM_PQ> scheduled;

};

inline void pqueue::schedule(uint64_t cycle, pqueue_type type)
{
    if(scheduled[type])
    {
        auto& c = q.container();
        for(auto& t : c)
        {
            if(t.type == type)
            {
                t.cycle = cycle;
                break;
            }
        }
        std::make_heap(c.begin(), c.end(), q.comparator());
    }
    else
    {
        q.push({ cycle, type });
        scheduled[type] = true;
    }
}

}
