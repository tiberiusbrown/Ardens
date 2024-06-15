#include "absim.hpp"

#include "absim_instructions.hpp"
#include "absim_timer.hpp"
#include "absim_adc.hpp"
#include "absim_pll.hpp"
#include "absim_spi.hpp"
#include "absim_eeprom.hpp"
#include "absim_w25q128.hpp"
#include "absim_sound.hpp"

#include <algorithm>

namespace absim
{

void atmega32u4_t::st_handle_prr0(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == 0x64);
    cpu.data[0x64] = x;
    cpu.adc_handle_prr0(x);
}

void atmega32u4_t::st_handle_pin(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr + 2] ^= x;
}

void atmega32u4_t::st_handle_port(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    // TODO: handle pullup behavior
    cpu.data[ptr] = x;
}

ARDENS_FORCEINLINE bool atmega32u4_t::check_interrupt(
    uint8_t vector, uint8_t flag, uint8_t& tifr)
{
    if(!flag) return false;
    //if(wakeup_cycles != 0) return false;
    assert(wakeup_cycles == 0);
    push_stack_frame(pc);
    push(uint8_t(pc >> 0));
    push(uint8_t(pc >> 8));
    pc = vector;
    tifr &= ~flag;
    sreg() &= ~SREG_I;
    wakeup_cycles = 4;
    if(!active)
        wakeup_cycles += 4;
    active = false;
    just_interrupted = true;
    return true;
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

ARDENS_FORCEINLINE uint32_t atmega32u4_t::advance_cycle()
{
    uint32_t cycles = 1;
    just_read = 0xffffffff;
    just_written = 0xffffffff;
    just_interrupted = false;
    bool single_instr_only = true;

    if(active)
    {
        // not sleeping: execute instruction(s)

        uint32_t max_merged_cycles = 1;

        single_instr_only = (
            spi_busy ||
            eeprom_busy ||
            pll_busy ||
            adc_busy ||
#ifndef ARDENS_NO_DEBUGGER
            no_merged ||
#endif
            false);

        if(!single_instr_only)
        {
            uint64_t t = timer0.next_update_cycle;
            t = std::min<uint64_t>(t, timer1.next_update_cycle);
            t = std::min<uint64_t>(t, timer3.next_update_cycle);
            t = std::min<uint64_t>(t, timer4.next_update_cycle);
            t = std::min<uint64_t>(t, usb_next_update_cycle);
            t = std::min<uint64_t>(t, spi_done_cycle);

            t -= cycle_count;
            single_instr_only |= (t < MAX_INSTR_CYCLES);
            max_merged_cycles = (uint32_t)std::min<uint64_t>(t, 1024) - MAX_INSTR_CYCLES;
        }

#ifndef ARDENS_NO_DEBUGGER
        executing_instr_pc = pc;
#endif
#ifndef ARDENS_NO_DEBUGGER
        uint16_t last_pc = last_addr / 2;
#endif
        if(single_instr_only)
        {
#ifndef ARDENS_NO_DEBUGGER
            if(pc >= last_pc)
            {
                autobreak(AB_OOB_PC);
                return cycles + 1;
            }
#endif
            auto const& i = decoded_prog[pc];
            prev_sreg = sreg();
            cycles = INSTR_MAP[i.func](*this, i);
            cycle_count += cycles;
        }
        else
        {
            uint64_t tcycles = cycle_count;
            uint64_t tcycles_max = tcycles + max_merged_cycles;
            do
            {
#ifndef ARDENS_NO_DEBUGGER
                if(pc >= last_pc)
                {
                    autobreak(AB_OOB_PC);
                    return cycles + 1;
                }
#endif
                auto const& i = merged_prog[pc];
                prev_sreg = sreg();
                auto instr_cycles = INSTR_MAP[i.func](*this, i);
                cycle_count += instr_cycles;
                if(should_autobreak() || just_written < 0x100 || just_read < 0x100)
                {
                    // need to check peripherals below
                    single_instr_only = true;
                    break;
                }
            } while(cycle_count < tcycles_max);
            cycles = uint32_t(cycle_count - tcycles);
        }
        //if(pc >= decoded_prog.size()) __debugbreak();
    }
    else if(wakeup_cycles > 0)
    {
        prev_sreg = sreg();
        // sleeping but waking up from an interrupt

#ifndef ARDENS_NO_DEBUGGER
        // set this here so we don't steal profiler cycle from
        // instruction that was running when interrupt hit
        if(wakeup_cycles == 4)
            executing_instr_pc = pc;
#endif

        if(--wakeup_cycles == 0)
            active = true;

        cycle_count += cycles;
    }
    else
    {
        prev_sreg = sreg();
        // sleeping and not waking up from an interrupt

        // boost executed cycles to speed up timer code
        if(spi_busy || eeprom_busy || pll_busy || adc_busy)
            cycles = 1;
        else
        {
            uint64_t t = timer0.next_update_cycle - cycle_count;
            t = std::min<uint64_t>(t, timer1.next_update_cycle - cycle_count);
            t = std::min<uint64_t>(t, timer3.next_update_cycle - cycle_count);
            t = std::min<uint64_t>(t, timer4.next_update_cycle - cycle_count);
            t = std::min<uint64_t>(t, usb_next_update_cycle - cycle_count);
            t = std::min<uint64_t>(t, 1024);
            if(t > 1) --t;
            cycles = (uint32_t)t;
        }
        single_instr_only = true;
        cycle_count += cycles;
    }

    if(single_instr_only)
    {
        // peripheral updates
        update_spi();
        cycle_pll(cycles);
        cycle_eeprom(cycles);
        cycle_adc(cycles);

        if(cycle_count >= timer0.next_update_cycle)
            update_timer0();
        if(cycle_count >= timer1.next_update_cycle)
            update_timer1();
        if(cycle_count >= timer3.next_update_cycle)
            update_timer3();
        if(cycle_count >= timer4.next_update_cycle)
            update_timer4();
        if(cycle_count >= usb_next_update_cycle)
            update_usb();

        do
        {
            if(!(prev_sreg & sreg() & SREG_I))
                break;

            // check for stack overflow only when interrupts are enabled
            check_stack_overflow();

            if(wakeup_cycles != 0) break;

            // handle interrupts here
            uint8_t i;

            // usb general
            i = (data[0xe1] & data[0xe2]) | (data[0xd8] & data[0xda]);
            if(i)
            {
                uint8_t dummy = 0;
                if(check_interrupt(0x14, i, dummy)) break;
            }

            // usb endpoint
            i = data[0xf4];
            if(check_interrupt(0x16, i, data[0xf4])) break;

            i = tifr1() & timsk1();
            if(i)
            {
                if(check_interrupt(0x22, i & 0x02, tifr1())) break; // TIMER1 COMPA
                if(check_interrupt(0x24, i & 0x04, tifr1())) break; // TIMER1 COMPB
                if(check_interrupt(0x26, i & 0x08, tifr1())) break; // TIMER1 COMPC
                if(check_interrupt(0x28, i & 0x01, tifr1())) break; // TIMER1 OVF
            }

            i = tifr0() & timsk0();
            if(i)
            {
                if(check_interrupt(0x2a, i & 0x02, tifr0())) break; // TIMER0 COMPA
                if(check_interrupt(0x2c, i & 0x04, tifr0())) break; // TIMER0 COMPB
                if(check_interrupt(0x2e, i & 0x01, tifr0())) break; // TIMER0 OVF
            }

            i = tifr3() & timsk3();
            if(i)
            {
                if(check_interrupt(0x40, i & 0x02, tifr3())) break; // TIMER3 COMPA
                if(check_interrupt(0x42, i & 0x04, tifr3())) break; // TIMER3 COMPB
                if(check_interrupt(0x44, i & 0x08, tifr3())) break; // TIMER3 COMPC
                if(check_interrupt(0x46, i & 0x01, tifr3())) break; // TIMER3 OVF
            }

            i = tifr4() & timsk4();
            if(i)
            {
                if(check_interrupt(0x4c, i & 0x40, tifr4())) break; // TIMER4 COMPA
                if(check_interrupt(0x4e, i & 0x20, tifr4())) break; // TIMER4 COMPB
                if(check_interrupt(0x50, i & 0x80, tifr4())) break; // TIMER4 COMPD
                if(check_interrupt(0x52, i & 0x04, tifr4())) break; // TIMER4 OVF
            }

        } while(0);
    }

    cycle_sound(cycles);

    return cycles;
}

void atmega32u4_t::update_all()
{
    update_timer0();
    update_timer1();
    update_timer3();
    update_timer4();
    update_usb();
}

}
