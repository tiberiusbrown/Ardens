#include "absim.hpp"

#include <algorithm>

namespace absim
{

// each cycle is 62.5 ns
constexpr uint64_t CYCLE_PS = 62500;

void arduboy_t::reset()
{
    profiler_reset();
    cpu.reset();
    display.reset();
    paused = false;

    if(cpu.breakpoints.test(0))
        paused = true;
}

void arduboy_t::profiler_reset()
{
    memset(&profiler_counts, 0, sizeof(profiler_counts));
    memset(&profiler_hotspots, 0, sizeof(profiler_hotspots));
    num_hotspots = 0;
    profiler_total = 0;
    profiler_total_with_sleep = 0;
    profiler_enabled = false;
}

void arduboy_t::profiler_build_hotspots()
{
    //
    // WARNING: extremely messy hacky heuristics here
    //

    std::bitset<NUM_INSTRS> starts;

    num_hotspots = 0;

    // identify hotspot starts
    uint32_t index = 0;
    for(uint32_t index = 0; index < cpu.num_instrs; ++index)
    {
        auto const& d = cpu.disassembled_prog[index];
        auto const& i = cpu.decoded_prog[d.addr / 2];
        
        // don't split on jumps/branches that are never taken
        if(profiler_counts[d.addr / 2] == 0)
            continue;

        bool call = (
            i.func == INSTR_CALL ||
            i.func == INSTR_RCALL ||
            i.func == INSTR_ICALL);
        bool conditional = false;

        if(index > 0)
        {
            auto const& dprev = cpu.disassembled_prog[index - 1];
            auto const& iprev = cpu.decoded_prog[dprev.addr / 2];
            switch(iprev.func)
            {
            case INSTR_SBRS:
            case INSTR_SBRC:
            case INSTR_SBIS:
            case INSTR_SBIC:
            case INSTR_CPSE:
                conditional = true;
                break;
            case INSTR_BRBC:
            case INSTR_BRBS:
                // previous instruction is a branch .+2 or .+4 (skip)
                conditional = (iprev.word == 1 || iprev.word == 2);
                break;
            default:
                break;
            }
        }

        if(i.func == INSTR_BRBS || i.func == INSTR_BRBC)
            conditional = true;

        int size = 1;
        int32_t target = -1;

        switch(i.func)
        {
        case INSTR_JMP:
        case INSTR_CALL:
            size = 2;
            target = i.word;
            break;
        case INSTR_RJMP:
        case INSTR_RCALL:
        case INSTR_BRBS:
        case INSTR_BRBC:
            //target = d.addr / 2 + 1 + (int16_t)i.word;
            target = 0;
            break;
        case INSTR_IJMP:
        case INSTR_RET:
        case INSTR_RETI:
            target = 0;
            break;
        default:
            break;
        }

        if(target < 0)
            continue;

        if(conditional)
            continue;

        if(!call)
            starts.set(index + size);
        if(target > 0)
            starts.set(target);
    }

    // now collect hotspots
    for(uint32_t start = 0, index = 1; index < cpu.num_instrs; ++index)
    {
        if(starts.test(index))
        {
            auto& h = profiler_hotspots[num_hotspots++];
            h.begin = start;
            h.end = index - 1;
            h.count = 0;
            for(int32_t i = h.begin; i <= h.end; ++i)
            {
                uint16_t addr = cpu.disassembled_prog[i].addr;
                h.count += profiler_counts[addr / 2];
            }
            if(h.count == 0) --num_hotspots;
            
            // trim from beginning
            for(int32_t i = h.begin; i <= h.end; ++i)
            {
                uint16_t addr = cpu.disassembled_prog[i].addr;
                if(profiler_counts[addr / 2] == 0)
                    ++h.begin;
                else
                    break;
            }

            // trim from end
            for(int32_t i = h.end; i >= h.begin; --i)
            {
                uint16_t addr = cpu.disassembled_prog[i].addr;
                if(profiler_counts[addr / 2] == 0)
                    --h.end;
                else
                    break;
            }

            // trim from middle: N+ consecutive zero-counts
            constexpr int N = 4;
            for(int32_t ns, n = 0, i = h.begin; i <= h.end; ++i)
            {
                uint16_t addr = cpu.disassembled_prog[i].addr;
                if(profiler_counts[addr / 2] == 0)
                    ++n;
                else if(n >= N)
                {
                    auto& hn = profiler_hotspots[num_hotspots++];
                    hn.begin = h.begin;
                    hn.end = ns - 1;
                    hn.count = 0;
                    for(int32_t j = hn.begin; j <= hn.end; ++j)
                    {
                        uint16_t hnaddr = cpu.disassembled_prog[j].addr;
                        hn.count += profiler_counts[hnaddr / 2];
                    }
                    h.count -= hn.count;
                    h.begin = i;
                    n = 0;
                }
                else
                    n = 0;
                if(n == 1) ns = i;
            }

            start = index;
        }
    }

    std::sort(
        profiler_hotspots.begin(),
        profiler_hotspots.begin() + num_hotspots,
        [](auto const& a, auto const& b) { return a.count > b.count; }
    );
}

void arduboy_t::cycle()
{
    if(!cpu.decoded) return;

    cpu.advance_cycle();

    // TODO: model SPI connection more precisely?
    // send SPI commands and data to display
    if(cpu.spi_done)
    {
        uint8_t byte = cpu.spi_data_byte;
        uint8_t portd = cpu.data[0x2b];
        if(!(portd & (1 << 6)))
        {
            if(portd & (1 << 4))
                display.send_data(byte);
            else
                display.send_command(byte);
        }
    }

    // each cycle is 62.5 ns
    constexpr uint64_t dps = 62500;

    display.advance(CYCLE_PS);

    if(profiler_enabled)
        ++profiler_total_with_sleep;
    if(profiler_enabled && (cpu.active || cpu.wakeup_cycles != 0))
    {
        if(cpu.executing_instr_pc < profiler_counts.size())
        {
            ++profiler_total;
            profiler_counts[cpu.executing_instr_pc] += 1;
        }
    }
}

void arduboy_t::advance_instr()
{
    if(!cpu.decoded) return;
    int n = 0;
    do
    {
        cpu.advance_cycle();
    } while(++n < 65536 && (!cpu.active || cpu.cycles_till_next_instr != 0));
}

void arduboy_t::advance(uint64_t ps)
{
    ps += ps_rem;
    ps_rem = 0;

    if(!cpu.decoded) return;
    if(paused) return;

    while(ps >= CYCLE_PS)
    {
        cycle();
        
        ps -= CYCLE_PS;

        if(cpu.pc < cpu.breakpoints.size() &&
            cpu.breakpoints.test(cpu.pc))
        {
            paused = true;
            return;
        }
    }

    // track remainder
    ps_rem = ps;
}

}
