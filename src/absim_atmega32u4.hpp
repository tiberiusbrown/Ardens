#include "absim.hpp"

#include "absim_instructions.hpp"
#include "absim_timer.hpp"
#include "absim_adc.hpp"
#include "absim_pll.hpp"
#include "absim_spi.hpp"
#include "absim_eeprom.hpp"
#include "absim_w25q128.hpp"

#include <algorithm>

namespace absim
{

void atmega32u4_t::update_sleep_min_cycles()
{
    // TODO: incorporate ADC conversion / EEPROM program cycles
    uint32_t d = 1024;
    if(timer0_divider != 0) d = std::min(d, timer0_divider);
    if(timer1_divider != 0) d = std::min(d, timer1_divider);
    if(timer3_divider != 0) d = std::min(d, timer3_divider);
    sleep_min_cycles = d;
}

FORCEINLINE void atmega32u4_t::check_interrupt(
    uint8_t vector, uint8_t flag, uint8_t& tifr)
{
    if(!flag) return;
    if(wakeup_cycles != 0) return;
    if(!(prev_sreg & SREG_I)) return;
    push(uint8_t(pc >> 0));
    push(uint8_t(pc >> 8));
    pc = vector;
    tifr &= ~flag;
    sreg() &= ~SREG_I;
    wakeup_cycles = 4;
    if(!active)
        wakeup_cycles += 4;
    active = false;
}

size_t atmega32u4_t::addr_to_disassembled_index(uint16_t addr)
{
    absim::disassembled_instr_t temp;
    temp.addr = addr;
    auto it = std::lower_bound(
        disassembled_prog.begin(),
        disassembled_prog.begin() + num_instrs,
        temp,
        [](auto const& a, auto const& b) { return a.addr < b.addr; }
    );

    auto index = std::distance(disassembled_prog.begin(), it);

    return (size_t)index;
}

FORCEINLINE uint32_t atmega32u4_t::advance_cycle()
{
    uint32_t cycles = 1;
    just_read = 0xffff;
    just_written = 0xffff;
    if(!active && wakeup_cycles > 0)
    {
        // sleeping but waking up from an interrupt

        // set this here so we don't steal profiler cycle from
        // instruction that was running when interrupt hit
        if(wakeup_cycles == 4)
            executing_instr_pc = pc;

        if(--wakeup_cycles == 0)
            active = true;
    }
    else if(!active)
    {
        // sleeping and not waking up from an interrupt

        // boost executed cycles to speed up timer code
        cycles = sleep_min_cycles;
    }
    
    if(active)
    {
        // not sleeping: try to execute instruction if one isn't still executing
        if(cycles_till_next_instr == 0)
        {
            if(pc >= decoded_prog.size())
                return cycles;
            auto const& i = decoded_prog[pc];
            if(i.func == INSTR_UNKNOWN)
                return cycles;
            executing_instr_pc = pc;
            prev_sreg = sreg();
            //cycles_till_next_instr = INSTR_MAP[i.func](*this, i);
            cycles = INSTR_MAP[i.func](*this, i);
        }
        //--cycles_till_next_instr;
    }

    spi_done = false;
    // peripheral updates
    cycle_spi(cycles);
    cycle_pll(cycles);
    cycle_timer0(cycles);
    cycle_timer1(cycles);
    cycle_timer3(cycles);
    cycle_eeprom(cycles);
    cycle_adc(cycles);

    // don't interrupt in the middle of a multi-cycle instruction
    if(cycles_till_next_instr == 0)
    {
        // handle interrupts here
        uint8_t i;

        i = tifr1() & timsk1();
        if(i)
        {
            check_interrupt(0x22, i & 0x02, tifr1()); // TIMER1 COMPA
            check_interrupt(0x24, i & 0x04, tifr1()); // TIMER1 COMPB
            check_interrupt(0x26, i & 0x08, tifr1()); // TIMER1 COMPC
            check_interrupt(0x28, i & 0x01, tifr1()); // TIMER1 OVF
        }

        i = tifr0() & timsk0();
        if(i)
        {
            check_interrupt(0x2a, i & 0x02, tifr0()); // TIMER0 COMPA
            check_interrupt(0x2c, i & 0x04, tifr0()); // TIMER0 COMPB
            check_interrupt(0x2e, i & 0x01, tifr0()); // TIMER0 OVF
        }

        i = tifr3() & timsk3();
        if(i)
        {
            check_interrupt(0x40, i & 0x02, tifr3()); // TIMER3 COMPA
            check_interrupt(0x42, i & 0x04, tifr3()); // TIMER3 COMPB
            check_interrupt(0x44, i & 0x08, tifr3()); // TIMER3 COMPC
            check_interrupt(0x46, i & 0x01, tifr3()); // TIMER3 OVF
        }
    }

    cycle_count += cycles;
    return cycles;
}

}
