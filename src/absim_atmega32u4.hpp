#include "absim.hpp"

#include "absim_instructions.hpp"
#include "absim_timer.hpp"
#include "absim_adc.hpp"
#include "absim_pll.hpp"
#include "absim_spi.hpp"
#include "absim_twi.hpp"
#include "absim_eeprom.hpp"
#include "absim_w25q128.hpp"
#include "absim_sound.hpp"
#include "absim_usb.hpp"

#include <algorithm>

namespace absim
{

void atmega32u4_t::st_handle_prr0(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::PRR0);
    sync_write_barrier(cpu);
    cpu.data[reg::addr::PRR0] = x;
    cpu.update_sync_timers();
    cpu.adc_handle_prr0(x);
    cpu.twi_handle_prr0(x);
}

void atmega32u4_t::st_handle_prr1(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::PRR1);
    sync_write_barrier(cpu);
    timer4_write_barrier(cpu);
    cpu.data[reg::addr::PRR1] = x;
    cpu.update_sync_timers();
    cpu.update_timer4();
}

void atmega32u4_t::st_handle_pin(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr + 2] ^= x;
    if(ptr == reg::addr::PINC)
        cpu.data[reg::addr::PINC] = cpu.data[reg::addr::PORTC];
}

void atmega32u4_t::st_handle_ddrd(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr] = x;
    if(cpu.twi_adapter)
        cpu.twi_adapter->sync_bus_lines();
}

void atmega32u4_t::st_handle_port(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr] = x;
    if(ptr == reg::addr::PORTC)
        cpu.data[reg::addr::PINC] = x;
    if(ptr == reg::addr::PORTD && cpu.twi_adapter)
        cpu.twi_adapter->sync_bus_lines();
}

void atmega32u4_t::st_handle_mcucr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    x &= ~reg::bit::MCUCR::IVCE;
    cpu.data[ptr] = x;
}

void atmega32u4_t::st_handle_mcusr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    x &= reg::bit::MCUSR::PORF |
        reg::bit::MCUSR::EXTRF |
        reg::bit::MCUSR::BORF |
        reg::bit::MCUSR::JTRF;
    x |= (cpu.data[reg::addr::MCUSR] & reg::bit::MCUSR::USBRF);
    if(x & reg::bit::MCUSR::WDRF)
        cpu.data[reg::addr::WDTCSR] |= reg::bit::WDTCSR::WDE;
    cpu.data[ptr] = x;
}

void atmega32u4_t::st_handle_wdtcsr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    // TODO: WDTON fuse
    // TODO: model WDCE behavior
    cpu.watchdog_divider_cycle = 0;
    x &= ~reg::bit::WDTCSR::WDIF;
    cpu.data[ptr] = x;
    cpu.update_watchdog();
}

void atmega32u4_t::update_watchdog_prescaler()
{
    uint8_t wdp = data[reg::addr::WDTCSR];
    wdp = (wdp &
        (reg::bit::WDTCSR::WDP0 |
         reg::bit::WDTCSR::WDP1 |
         reg::bit::WDTCSR::WDP2)) |
        ((wdp & reg::bit::WDTCSR::WDP3) >> 2);
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
        uint8_t csr = data[reg::addr::WDTCSR];
        if(csr & reg::bit::WDTCSR::WDIE)
        {
            // interrupt
            data[reg::addr::WDTCSR] |= reg::bit::WDTCSR::WDIF;
            schedule_interrupt_check();
            if(csr & reg::bit::WDTCSR::WDE)
            {
                // if also system reset, disable interrupt
                data[reg::addr::WDTCSR] &= ~reg::bit::WDTCSR::WDIE;
            }
        }
        else if(csr & reg::bit::WDTCSR::WDE)
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
    peripheral_queue.schedule(watchdog_next_cycle, PQ_WATCHDOG);
}

void atmega32u4_t::st_handle_spmcsr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.update_spm();
    if(cpu.spm_op != cpu.SPM_OP_NONE)
        x |= reg::bit::SPMCSR::SPMEN;
    else
    {
        uint8_t const mask =
            reg::bit::SPMCSR::SPMEN |
            reg::bit::SPMCSR::PGERS |
            reg::bit::SPMCSR::PGWRT |
            reg::bit::SPMCSR::BLBSET |
            reg::bit::SPMCSR::RWWSRE |
            reg::bit::SPMCSR::SIGRD;
        if((x & mask) ==
            (reg::bit::SPMCSR::SPMEN | reg::bit::SPMCSR::PGERS))
            cpu.spm_op = cpu.SPM_OP_PAGE_ERASE;
        else if((x & mask) ==
            (reg::bit::SPMCSR::SPMEN | reg::bit::SPMCSR::PGWRT))
            cpu.spm_op = cpu.SPM_OP_PAGE_WRITE;
        else if((x & mask) ==
            (reg::bit::SPMCSR::SPMEN | reg::bit::SPMCSR::BLBSET))
            cpu.spm_op = cpu.SPM_OP_BLB_SET;
        else if((x & mask) ==
            (reg::bit::SPMCSR::SPMEN | reg::bit::SPMCSR::RWWSRE))
        {
            cpu.spm_op = cpu.SPM_OP_RWW_EN;
            cpu.erase_spm_buffer();
        }
        else if((x & mask) ==
            (reg::bit::SPMCSR::SPMEN | reg::bit::SPMCSR::SIGRD))
            cpu.spm_op = cpu.SPM_OP_SIG_READ;
        else if((x & mask) == reg::bit::SPMCSR::SPMEN)
            cpu.spm_op = cpu.SPM_OP_PAGE_LOAD;
        else
            return; // no effect
        cpu.spm_busy = true;
        cpu.spm_en_cycles = 5;
        cpu.peripheral_queue.schedule(cpu.cycle_count + cpu.spm_en_cycles, PQ_SPM);
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
        data[reg::addr::SPMCSR] |= reg::bit::SPMCSR::RWWSB;
        decode();
        last_addr = 0x7fff;
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
        data[reg::addr::SPMCSR] |= reg::bit::SPMCSR::RWWSB;
        decode();
        last_addr = 0x7fff;
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
        data[reg::addr::SPMCSR] &= ~reg::bit::SPMCSR::RWWSB;
        break;
    }
    default:
        break;
    }
    if(spm_cycles != 0)
        peripheral_queue.schedule(cycle_count + spm_cycles, PQ_SPM);
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
        {
            spm_cycles -= cycles;
            peripheral_queue.schedule(cycle_count + spm_cycles, PQ_SPM);
        }
    }
    else
    {
        assert(spm_en_cycles != 0);
        if(spm_en_cycles <= cycles)
            spm_busy = false;
        else
        {
            spm_en_cycles -= cycles;
            peripheral_queue.schedule(cycle_count + spm_en_cycles, PQ_SPM);
        }
    }

    if(!spm_busy)
    {
        spm_op = SPM_OP_NONE;
        spm_en_cycles = 0;
        spm_cycles = 0;
        data[reg::addr::SPMCSR] &= (reg::bit::SPMCSR::RWWSB | reg::bit::SPMCSR::SPMIE);
    }
}

inline void atmega32u4_t::schedule_interrupt_check()
{
    peripheral_queue.schedule(cycle_count, PQ_INTERRUPT);
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
    if(vector != 0 && (data[reg::addr::MCUCR] & reg::bit::MCUCR::IVSEL) != 0)
        pc = vector + bootloader_address();
    else
        pc = vector;
    tifr &= ~flag;
    data[reg::addr::SREG] &= ~reg::bit::SREG::I;
    wakeup_cycles = 4;
    if(!active)
        wakeup_cycles += 4;
    active = false;
    just_interrupted = true;
    return true;
}

ARDENS_FORCEINLINE void atmega32u4_t::check_all_interrupts()
{
    auto& sreg = data[reg::addr::SREG];
    auto& udint = data[reg::addr::UDINT];
    auto& udien = data[reg::addr::UDIEN];
    auto& usbint = data[reg::addr::USBINT];
    auto& usbc = data[reg::addr::USBCON];
    auto& ueint = data[reg::addr::UEINT];
    auto& wdtcsr = data[reg::addr::WDTCSR];
    auto& tifr1 = data[reg::addr::TIFR1];
    auto& timsk1 = data[reg::addr::TIMSK1];
    auto& tifr0 = data[reg::addr::TIFR0];
    auto& timsk0 = data[reg::addr::TIMSK0];
    auto& tifr3 = data[reg::addr::TIFR3];
    auto& timsk3 = data[reg::addr::TIMSK3];
    auto& tifr4 = data[reg::addr::TIFR4];
    auto& timsk4 = data[reg::addr::TIMSK4];

    if(!(prev_sreg & sreg & reg::bit::SREG::I))
    {
        if(sreg & reg::bit::SREG::I)
            peripheral_queue.schedule(cycle_count + 1, PQ_INTERRUPT);
        return;
    }

    // check for stack overflow only when interrupts are enabled
    check_stack_overflow();

    if(wakeup_cycles != 0) return;

    // handle interrupts here
    uint8_t i;

    // usb general
    i = (udint & udien) | (usbint & usbc);
    if(i)
    {
        uint8_t dummy = 0;
        if(check_interrupt(0x14, i, dummy)) return;
    }

    // usb endpoint
    i = ueint;
    if(check_interrupt(0x16, i, ueint)) return;

    // watchdog timeout
    if(check_interrupt(0x18, wdtcsr & reg::bit::WDTCSR::WDIF, wdtcsr)) return;

    i = tifr1 & timsk1;
    if(i)
    {
        if(check_interrupt(0x22, i & reg::bit::TIFR1::OCF1A, tifr1)) return; // TIMER1 COMPA
        if(check_interrupt(0x24, i & reg::bit::TIFR1::OCF1B, tifr1)) return; // TIMER1 COMPB
        if(check_interrupt(0x26, i & reg::bit::TIFR1::OCF1C, tifr1)) return; // TIMER1 COMPC
        if(check_interrupt(0x28, i & reg::bit::TIFR1::TOV1, tifr1)) return; // TIMER1 OVF
    }

    i = tifr0 & timsk0;
    if(i)
    {
        if(check_interrupt(0x2a, i & reg::bit::TIFR0::OCF0A, tifr0)) return; // TIMER0 COMPA
        if(check_interrupt(0x2c, i & reg::bit::TIFR0::OCF0B, tifr0)) return; // TIMER0 COMPB
        if(check_interrupt(0x2e, i & reg::bit::TIFR0::TOV0, tifr0)) return; // TIMER0 OVF
    }

    i = tifr3 & timsk3;
    if(i)
    {
        if(check_interrupt(0x40, i & reg::bit::TIFR3::OCF3A, tifr3)) return; // TIMER3 COMPA
        if(check_interrupt(0x42, i & reg::bit::TIFR3::OCF3B, tifr3)) return; // TIMER3 COMPB
        if(check_interrupt(0x44, i & reg::bit::TIFR3::OCF3C, tifr3)) return; // TIMER3 COMPC
        if(check_interrupt(0x46, i & reg::bit::TIFR3::TOV3, tifr3)) return; // TIMER3 OVF
    }

    auto const& twcr = data[reg::addr::TWCR];
    i = (twcr & (reg::bit::TWCR::TWINT | reg::bit::TWCR::TWIE)) ==
        (reg::bit::TWCR::TWINT | reg::bit::TWCR::TWIE) ?
        reg::bit::TWCR::TWINT :
        0;
    if(i)
    {
        uint8_t dummy = i;
        if(check_interrupt(0x48, i, dummy)) return; // TWI
    }

    i = tifr4 & timsk4;
    if(i)
    {
        if(check_interrupt(0x4c, i & reg::bit::TIFR4::OCF4A, tifr4)) return; // TIMER4 COMPA
        if(check_interrupt(0x4e, i & reg::bit::TIFR4::OCF4B, tifr4)) return; // TIMER4 COMPB
        if(check_interrupt(0x50, i & reg::bit::TIFR4::OCF4D, tifr4)) return; // TIMER4 COMPD
        if(check_interrupt(0x52, i & reg::bit::TIFR4::TOV4, tifr4)) return; // TIMER4 OVF
    }
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
    just_interrupted = false;
    auto& sreg = data[reg::addr::SREG];

    constexpr uint64_t MAX_MERGED_CYCLES = 1024;

    // peripheral updates
    for(;;)
    {
        auto qi = peripheral_queue.next();
        if(qi.cycle > cycle_count)
            break;
        peripheral_queue.pop();
        switch(qi.type)
        {
        case PQ_SPI: update_spi(); break;
        case PQ_TIMER_SYNC: update_sync_timers(); break;
        case PQ_TIMER1: break;
        case PQ_TIMER3: break;
        case PQ_TIMER4: update_timer4(); break;
        case PQ_USB: update_usb(); break;
        case PQ_WATCHDOG: update_watchdog(); break;
        case PQ_EEPROM: update_eeprom(); break;
        case PQ_PLL: update_pll(); break;
        case PQ_ADC: update_adc(); break;
        case PQ_SPM: update_spm(); break;
        case PQ_TWI: update_twi(); break;
        case PQ_INTERRUPT: check_all_interrupts(); break;
        default: break;
        }
    }

    if(active)
    {
        // not sleeping: execute instruction(s)

        int64_t max_merged_cycles = int64_t(
            peripheral_queue.next_cycle() - cycle_count - MAX_INSTR_CYCLES);

#ifndef ARDENS_NO_DEBUGGER
        executing_instr_pc = pc;
#endif
        constexpr uint16_t last_pc = 0x4000;
        if(max_merged_cycles < 0 ||
#ifndef ARDENS_NO_DEBUGGER
            no_merged ||
#endif
            false)
        {
            just_read = 0xffffffff;
            just_written = 0xffffffff;
            if(pc >= last_pc)
            {
                autobreak(AB_OOB_PC);
                return 1;
            }
            auto const& i = decoded_prog[pc];
            prev_sreg = sreg;
            cycles = INSTR_MAP[i.func](*this, i);
            assert(cycles <= MAX_INSTR_CYCLES);
            cycle_count += cycles;
        }
        else
        {
            uint64_t tcycles = cycle_count;
            int64_t cycles_max = std::min<uint64_t>(
                max_merged_cycles, MAX_MERGED_CYCLES);

            // this can happen here because if SREG I-bit is ever changed
            // it'll break out of the loop anyway
            prev_sreg = sreg;
            
            io_reg_accessed = false;
            do
            {
                if(pc >= last_pc)
                {
                    autobreak(AB_OOB_PC);
                    return 1;
                }
                auto const& i = merged_prog[pc];
                auto const old_sreg = sreg;
                auto instr_cycles = INSTR_MAP[i.func](*this, i);
                bool const i_changed = (old_sreg ^ sreg) & reg::bit::SREG::I;
                assert(instr_cycles <= MAX_INSTR_CYCLES);
                cycle_count += instr_cycles;
                if(!active || i_changed || io_reg_accessed || should_autobreak())
                    break;
                cycles_max -= instr_cycles;
            } while((int64_t)cycles_max > 0);
            cycles = uint32_t(cycle_count - tcycles);
        }
    }
    else if(wakeup_cycles > 0)
    {
        prev_sreg = sreg;
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
        // sleeping and not waking up from an interrupt
        prev_sreg = sreg;

        // boost executed cycles to speed up timer code
        uint64_t t = std::max(cycle_count + 1, peripheral_queue.next_cycle());
        t -= cycle_count;
        t = std::min<uint64_t>(t, MAX_MERGED_CYCLES);
        cycles = (uint32_t)t;
        cycle_count += cycles;
    }

    // if interrupts were just enabled, schedule interrupt check for next cycle
    // (allows next instruction to execute)
    if(~prev_sreg & sreg & reg::bit::SREG::I)
        peripheral_queue.schedule(cycle_count + 1, PQ_INTERRUPT);

    update_sound();

    return cycles;
}

void atmega32u4_t::update_all()
{
    update_sync_timers();
    update_timer4();
    update_usb();
    update_twi();
    update_sound();
}

}
