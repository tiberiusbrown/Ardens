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
    if(timer1.divider != 0) d = std::min(d, timer1.divider);
    if(timer3.divider != 0) d = std::min(d, timer3.divider);
    sleep_min_cycles = d;
}

void atmega32u4_t::st_handle_prr0(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == 0x64);
    cpu.data[0x64] = x;
    cpu.adc_handle_prr0(x);
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
        // not sleeping: execute instruction
        if(pc >= decoded_prog.size())
            return cycles;
        auto const& i = decoded_prog[pc];
        if(i.func == INSTR_UNKNOWN)
            return cycles;
        executing_instr_pc = pc;
        prev_sreg = sreg();
        cycles = INSTR_MAP[i.func](*this, i);
    }
    cycle_count += cycles;

    spi_done = false;
    // peripheral updates
    cycle_spi(cycles);
    cycle_pll(cycles);
    cycle_eeprom(cycles);
    cycle_adc(cycles);

    cycle_timer0(cycles);

    if(cycle_count >= timer1.next_update_cycle)
        update_timer1();
    if(cycle_count >= timer3.next_update_cycle)
        update_timer3();

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

    return cycles;
}

void atmega32u4_t::update_all()
{
    update_timer1();
    update_timer3();
}

}
