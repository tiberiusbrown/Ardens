#include "absim.hpp"

#include "absim_instructions.hpp"

namespace absim
{

void atmega32u4_t::check_interrupt(
    uint8_t vector, uint8_t flag, uint8_t& tifr)
{
    if(interrupting) return;
    if(!flag) return;
    push(uint8_t(pc >> 0));
    push(uint8_t(pc >> 8));
    pc = vector;
    tifr &= ~flag;
    sreg() &= ~SREG_I;
    interrupting = true;
    wakeup_cycles = 4;
    if(!active)
        wakeup_cycles += 4;
    active = false;
}

void atmega32u4_t::advance_cycle()
{
    interrupting = false;
    just_read = 0xffff;
    just_written = 0xffff;
    if(!active && wakeup_cycles > 0)
    {
        // set this here so we don't steal profiler cycle from
        // instruction that was running when interrupt hit
        if(wakeup_cycles == 4)
            executing_instr_pc = pc;

        if(--wakeup_cycles == 0)
            active = true;
    }
    if(active)
    {
        if(cycles_till_next_instr == 0)
        {
            if(pc >= decoded_prog.size())
                return;
            auto const& i = decoded_prog[pc];
            if(i.func == INSTR_UNKNOWN)
                return;
            executing_instr_pc = pc;
            prev_sreg = sreg();
            cycles_till_next_instr = INSTR_MAP[i.func](*this, i);
        }
        --cycles_till_next_instr;
    }
    spi_done = false;

    // peripheral updates
    cycle_spi();
    cycle_pll();
    cycle_timer0();
    cycle_eeprom();

    if(cycles_till_next_instr == 0 && wakeup_cycles == 0 && (prev_sreg & SREG_I))
    {
        // handle interrupts here
        uint8_t i;

        i = tifr0() & timsk0();
        if(i)
        {
            check_interrupt(0x2e, i & 0x01, tifr0()); // TIMER0 OVF
            check_interrupt(0x2a, i & 0x02, tifr0()); // TIMER0 COMPA
            check_interrupt(0x2c, i & 0x04, tifr0()); // TIMER0 COMPB
        }
    }
}

}
