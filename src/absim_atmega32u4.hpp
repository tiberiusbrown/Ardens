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

void atmega32u4_t::st_handle_mcucr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    x &= 0xfe;
    cpu.data[ptr] = x;
}

void atmega32u4_t::st_handle_mcusr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    x &= 0x17;
    x |= (cpu.MCUSR() & 0xe0);
    if(x & (1 << 3))
        cpu.WDTCSR() |= (1 << 3);
    cpu.data[ptr] = x;
}

void atmega32u4_t::st_handle_wdtcsr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    // TODO: WDTON fuse
    // TODO: model WDCE behavior
    cpu.watchdog_divider_cycle = 0;
    x &= 0x7f; // clear interrupt flag
    cpu.data[ptr] = x;
    cpu.update_watchdog();
}

void atmega32u4_t::update_watchdog_prescaler()
{
    uint8_t wdp = WDTCSR();
    wdp = (wdp & 7) | ((wdp >> 2) & 8);
    if(wdp > 9) wdp = 9;

    // 128 kHz osc divider
    watchdog_divider = (16000000 / 128000) << (wdp + 11);
}

void atmega32u4_t::update_watchdog()
{
    uint32_t cycles = uint32_t(cycle_count - watchdog_prev_cycle);
    watchdog_prev_cycle = cycle_count;

    uint32_t wdt_hits = increase_counter(watchdog_divider_cycle, cycles, watchdog_divider);

    if(wdt_hits != 0)
    {
        // watchdog timeout
        uint8_t csr = WDTCSR();
        if(csr & (1 << 6))
        {
            // interrupt
            WDTCSR() |= (1 << 7);
            if(csr & (1 << 3))
            {
                // if also system reset, disable interrupt
                WDTCSR() &= ~(1 << 6);
            }
        }
        else if(csr & (1 << 3))
        {
            // system reset
            soft_reset();
            return;
        }
    }

    update_watchdog_prescaler();

    // normalize divider cycle in case divider decreased
    watchdog_divider_cycle %= watchdog_divider;

    // schedule next update
    watchdog_next_cycle = watchdog_prev_cycle + watchdog_divider - watchdog_divider_cycle;
}

void atmega32u4_t::st_handle_spmcsr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.update_spm();
    if(cpu.spm_op != cpu.SPM_OP_NONE)
        x |= 0x01;
    else
    {
        if((x & 0x3f) == (1 << 1) + 1)
            cpu.spm_op = cpu.SPM_OP_PAGE_ERASE;
        else if((x & 0x3f) == (1 << 2) + 1)
            cpu.spm_op = cpu.SPM_OP_PAGE_WRITE;
        else if((x & 0x3f) == (1 << 3) + 1)
            cpu.spm_op = cpu.SPM_OP_BLB_SET;
        else if((x & 0x3f) == (1 << 4) + 1)
        {
            cpu.spm_op = cpu.SPM_OP_RWW_EN;
            cpu.erase_spm_buffer();
        }
        else if((x & 0x3f) == (1 << 5) + 1)
            cpu.spm_op = cpu.SPM_OP_SIG_READ;
        else if((x & 0x3f) == 1)
            cpu.spm_op = cpu.SPM_OP_PAGE_LOAD;
        else
            return; // no effect
        cpu.spm_busy = true;
        cpu.spm_en_cycles = 5;
    }
    cpu.data[ptr] = x;
}

void atmega32u4_t::execute_spm()
{
    constexpr uint32_t SPM_CYCLES = 16000000 / 250; // 4ms
    switch(spm_op)
    {
    case SPM_OP_PAGE_LOAD:
    {
        // address in Z-reg
        uint8_t i = data[30] & 0x7e;
        spm_buffer[i + 0] &= data[0];
        spm_buffer[i + 1] &= data[1];
        break;
    }
    case SPM_OP_PAGE_ERASE:
    {
        if(spm_cycles != 0)
            break; // TODO: autobreak
        uint32_t a = std::min<uint32_t>(PROG_SIZE_BYTES, z_word() & 0xff80);
        uint32_t b = std::min<uint32_t>(PROG_SIZE_BYTES, a + 128);
        if(a >= bootloader_address() * 2u)
            break; // TODO: autobreak / this should halt the device
        for(uint32_t i = a; i < b; ++i)
            prog[i] = 0xff;
        SPMCSR() |= (1 << 6); // RWWSB
        decode();
        spm_cycles = SPM_CYCLES;
        break;
    }
    case SPM_OP_PAGE_WRITE:
    {
        if(spm_cycles != 0)
            break; // TODO: autobreak
        uint32_t a = std::min<uint32_t>(PROG_SIZE_BYTES, z_word() & 0xff80);
        uint32_t b = std::min<uint32_t>(PROG_SIZE_BYTES, a + 128);
        if(a >= bootloader_address() * 2u)
            break; // TODO: autobreak / this should halt the device
        for(uint32_t i = a; i < b; ++i)
            prog[i] &= spm_buffer[i - a];
        erase_spm_buffer();
        SPMCSR() |= (1 << 6); // RWWSB
        decode();
        spm_cycles = SPM_CYCLES;
        break;
    }
    case SPM_OP_BLB_SET:
    {
        // TODO: complete this
        if(spm_cycles != 0)
            break; // TODO: autobreak
        spm_cycles = SPM_CYCLES;
        break;
    }
    case SPM_OP_RWW_EN:
    {
        SPMCSR() &= ~(1 << 6); // RWWSB
        break;
    }
    default:
        break;
    }
}

void atmega32u4_t::update_spm()
{
    if(!spm_busy)
    {
        spm_prev_cycle = cycle_count;
        return;
    }

    uint32_t cycles = uint32_t(cycle_count - spm_prev_cycle);
    spm_prev_cycle = cycle_count;

    if(spm_cycles != 0)
    {
        spm_en_cycles = 0;
        if(spm_cycles <= cycles)
            spm_busy = false;
        else
            spm_cycles -= cycles;
    }
    else if(spm_en_cycles != 0)
    {
        if(spm_en_cycles <= cycles)
            spm_busy = false;
        else
            spm_en_cycles -= cycles;
    }

    if(!spm_busy)
    {
        spm_op = SPM_OP_NONE;
        spm_en_cycles = 0;
        spm_cycles = 0;
        SPMCSR() &= 0xc0;
    }
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
    if(vector != 0 && (MCUCR() & (1 << 1)) != 0)
        pc = vector + bootloader_address();
    else
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
        disassembled_prog.begin() + num_instrs_total,
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
            spm_busy ||
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
            t = std::min<uint64_t>(t, watchdog_next_cycle);

            t -= cycle_count;
            single_instr_only |= (t < MAX_INSTR_CYCLES);
            max_merged_cycles = (uint32_t)std::min<uint64_t>(t, 1024) - MAX_INSTR_CYCLES;
        }

#ifndef ARDENS_NO_DEBUGGER
        executing_instr_pc = pc;
#endif
#ifndef ARDENS_NO_DEBUGGER
        constexpr uint16_t last_pc = 0x4000;
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
        if(spi_busy || eeprom_busy || pll_busy || adc_busy || spm_busy)
            cycles = 1;
        else
        {
            uint64_t t = timer0.next_update_cycle - cycle_count;
            t = std::min<uint64_t>(t, timer1.next_update_cycle - cycle_count);
            t = std::min<uint64_t>(t, timer3.next_update_cycle - cycle_count);
            t = std::min<uint64_t>(t, timer4.next_update_cycle - cycle_count);
            t = std::min<uint64_t>(t, usb_next_update_cycle - cycle_count);
            t = std::min<uint64_t>(t, spi_done_cycle);
            t = std::min<uint64_t>(t, watchdog_next_cycle);
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
        update_spm();

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
        if(cycle_count >= watchdog_next_cycle)
            update_watchdog();

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

            // watchdog timeout
            if(check_interrupt(0x18, WDTCSR() & 0x80, WDTCSR())) break;

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
