#include "absim.hpp"

namespace absim
{

static constexpr uint32_t SYNC_PRESCALER_PERIOD = 1024;

void atmega32u4_t::st_handler_timsk(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr] = x;
    cpu.schedule_interrupt_check();
}

ARDENS_FORCEINLINE static uint32_t word(atmega32u4_t const& cpu, uint32_t addr)
{
    uint32_t lo = cpu.data[addr + 0];
    uint32_t hi = cpu.data[addr + 1];
    return lo | (hi << 8);
}

ARDENS_FORCEINLINE static void set_word(atmega32u4_t& cpu, uint32_t addr, uint32_t x)
{
    cpu.data[addr + 0] = uint8_t(x >> 0);
    cpu.data[addr + 1] = uint8_t(x >> 8);
}

ARDENS_FORCEINLINE static uint32_t get_sync_divider(uint32_t cs)
{
    assert((cs & ~7) == 0);
    static constexpr uint32_t DIVIDERS[8] =
    {
        0, 1, 8, 64, 256, 1024, 0, 0,
    };
    return DIVIDERS[cs];
}

ARDENS_FORCEINLINE static uint64_t min_nonzero64(uint64_t a, uint64_t b)
{
    if(b == 0) return a;
    return std::min(a, b);
}

ARDENS_FORCEINLINE static uint32_t min_nonzero(uint32_t a, uint32_t top, int32_t b)
{
    if(b <= 0) return std::min(a, top);
    return std::min(a, (uint32_t)b);
}

ARDENS_FORCEINLINE static bool sync_tsm(atmega32u4_t const& cpu)
{
    return (cpu.data[reg::addr::GTCCR] & reg::bit::GTCCR::TSM) != 0;
}

ARDENS_FORCEINLINE static void set_timer_flags(
    atmega32u4_t& cpu, uint32_t tifr_addr, uint8_t flags)
{
    if(flags == 0)
        return;
    cpu.data[tifr_addr] |= flags;
    cpu.schedule_interrupt_check();
}

ARDENS_FORCEINLINE static uint64_t ticks_from_sync_cycles(
    uint32_t prescaler_phase, uint64_t cycles, uint32_t divider)
{
    if(divider == 0)
        return 0;
    if(divider == 1)
        return cycles;
    return ((prescaler_phase % divider) + cycles) / divider;
}

ARDENS_FORCEINLINE static uint64_t cycles_until_sync_ticks(
    uint32_t prescaler_phase, uint32_t divider, uint64_t ticks)
{
    if(ticks == 0 || divider == 0)
        return UINT64_MAX;
    if(divider == 1)
        return ticks;
    uint64_t first = divider - (prescaler_phase % divider);
    return first + (ticks - 1) * divider;
}

ARDENS_FORCEINLINE static void process_wgm8(
    uint32_t wgm, uint32_t& top, uint32_t& tov, uint32_t ocr)
{
    top = 0xff;
    tov = 0xff;
    
    switch(wgm)
    {
    case 0x0: // normal
        break;
    case 0x1: // PWM, phase correct
        tov = 0x00;
        break;
    case 0x2: // CTC
        top = ocr;
        break;
    case 0x3: // fast PWM
        break;
    case 0x5: // PWM, phase correct
        tov = 0x00;
        top = ocr;
        break;
    case 0x7: // fast PWM
        top = ocr;
        break;
    default:
        break;
    }

    if((wgm & 1) == 1)
        top = std::max<uint32_t>(3, top);
}

ARDENS_FORCEINLINE static uint32_t timer0_wgm(atmega32u4_t const& cpu)
{
    uint32_t tccr0a = cpu.data[reg::addr::TCCR0A];
    uint32_t tccr0b = cpu.data[reg::addr::TCCR0B];
    return (tccr0a & 0x3) | ((tccr0b >> 1) & 0x4);
}

ARDENS_FORCEINLINE static bool timer0_powered(atmega32u4_t const& cpu)
{
    return (cpu.data[reg::addr::PRR0] & reg::bit::PRR0::PRTIM0) == 0;
}

ARDENS_FORCEINLINE static bool timer0_running(atmega32u4_t const& cpu)
{
    return timer0_powered(cpu) && !sync_tsm(cpu) && cpu.timer0.divider != 0;
}

static void timer0_refresh_config(atmega32u4_t& cpu, bool force_active)
{
    auto& timer = cpu.timer0;
    uint32_t old_divider = timer.divider;
    uint32_t cs = cpu.data[reg::addr::TCCR0B] & 0x7;
    uint32_t wgm = timer0_wgm(cpu);
    uint32_t wgm_mask = 1u << wgm;

    timer.divider = get_sync_divider(cs);
    timer.ocrNa_buffer = cpu.data[reg::addr::OCR0A];
    timer.ocrNb_buffer = cpu.data[reg::addr::OCR0B];
    timer.update_ocrN_at_top = (wgm_mask & 0xaa) != 0;
    timer.phase_correct = (wgm_mask & 0x22) != 0;
    timer.fast_pwm = (wgm_mask & 0x88) != 0;

    bool immediate = (wgm_mask & 0x55) != 0;
    bool stopped = !timer0_powered(cpu) || timer.divider == 0;
    if(force_active || old_divider == 0 || stopped || immediate)
    {
        timer.ocrNa = timer.ocrNa_buffer;
        timer.ocrNb = timer.ocrNb_buffer;
    }

    if(timer.update_ocrN_at_top && timer.tcnt == timer.top)
    {
        timer.ocrNa = timer.ocrNa_buffer;
        timer.ocrNb = timer.ocrNb_buffer;
    }

    process_wgm8(wgm, timer.top, timer.tov, timer.ocrNa);
    if(!timer.phase_correct)
        timer.count_down = false;
}

ARDENS_FORCEINLINE static void timer0_tick(
    atmega32u4_t& cpu, bool compare_allowed, uint8_t& tifr)
{
    auto& timer = cpu.timer0;
    uint32_t tcnt = timer.tcnt;

    if(timer.count_down)
    {
        if(tcnt != 0)
            --tcnt;
        if(tcnt == 0)
            tifr |= reg::bit::TIFR0::TOV0, timer.count_down = false;
    }
    else if(tcnt > timer.top)
    {
        tcnt = (tcnt + 1) & 0xff;
        if(timer.top == 0xff && tcnt == 0)
            tifr |= reg::bit::TIFR0::TOV0;
    }
    else
    {
        ++tcnt;
        if(tcnt == timer.top + 1)
        {
            if(timer.phase_correct)
                timer.count_down = true, tcnt -= 2;
            else
            {
                if(timer.top == 0xff || timer.fast_pwm)
                    tifr |= reg::bit::TIFR0::TOV0;
                tcnt = 0;
            }
        }
    }

    timer.tcnt = tcnt & 0xff;
    if(compare_allowed)
    {
        if(timer.tcnt == timer.ocrNa) tifr |= reg::bit::TIFR0::OCF0A;
        if(timer.tcnt == timer.ocrNb) tifr |= reg::bit::TIFR0::OCF0B;
    }
}

static void timer0_advance_ticks(atmega32u4_t& cpu, uint64_t ticks)
{
    auto& timer = cpu.timer0;
    uint8_t tifr = 0;

    if(timer.compare_block_next_tick && ticks != 0)
    {
        timer0_tick(cpu, false, tifr);
        timer.compare_block_next_tick = false;
        --ticks;
    }

    uint32_t tcnt = timer.tcnt;
    bool count_down = timer.count_down;
    uint32_t ocrNa = timer.ocrNa;
    uint32_t ocrNb = timer.ocrNb;
    uint32_t top = timer.top;

    while(ticks > 0)
    {
        if(count_down)
        {
            uint32_t stop = 0;
            if(ocrNa < tcnt) stop = std::max(stop, ocrNa);
            if(ocrNb < tcnt) stop = std::max(stop, ocrNb);
            uint64_t t = tcnt - stop;
            t = std::min(t, ticks);
            ticks -= t;
            tcnt -= uint32_t(t);
            if(tcnt == 0)
                tifr |= reg::bit::TIFR0::TOV0, count_down = false;
        }
        else if(tcnt > top)
        {
            uint64_t t = 256;
            t = std::min(t, ticks);
            ticks -= t;
            tcnt += uint32_t(t);
            tcnt &= 0xff;
            if(top == 0xff)
                tifr |= reg::bit::TIFR0::TOV0;
        }
        else
        {
            uint32_t stop = top + 1;
            if(ocrNa > tcnt) stop = std::min(stop, ocrNa);
            if(ocrNb > tcnt) stop = std::min(stop, ocrNb);
            uint64_t t = stop - tcnt;
            t = std::min(t, ticks);
            ticks -= t;
            tcnt += uint32_t(t);
            if(tcnt == top + 1)
            {
                if(timer.phase_correct)
                    count_down = true, tcnt -= 2;
                else
                {
                    if(top == 0xff || timer.fast_pwm)
                        tifr |= reg::bit::TIFR0::TOV0;
                    tcnt = 0;
                }
            }
        }
        if(tcnt == ocrNa) tifr |= reg::bit::TIFR0::OCF0A;
        if(tcnt == ocrNb) tifr |= reg::bit::TIFR0::OCF0B;
    }

    timer.tcnt = tcnt & 0xff;
    timer.count_down = count_down;
    cpu.data[reg::addr::TCNT0] = uint8_t(timer.tcnt);
    set_timer_flags(cpu, reg::addr::TIFR0, tifr);
}

static uint64_t timer0_next_ticks(atmega32u4_t const& cpu)
{
    auto const& timer = cpu.timer0;
    if(!timer0_running(cpu))
        return UINT64_MAX;
    if(timer.compare_block_next_tick)
        return 1;

    uint32_t update_tcycles = UINT32_MAX;
    if(timer.count_down)
    {
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt - timer.ocrNa);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt - timer.ocrNb);
    }
    else
    {
        if(timer.phase_correct)
            update_tcycles = min_nonzero(update_tcycles, timer.top, timer.top - timer.tcnt);
        else
            update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tov - timer.tcnt + 1);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNa - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNb - timer.tcnt);
        if(timer.tcnt == timer.top && timer.top != 0)
            update_tcycles = 1;
    }
    if(update_tcycles == UINT32_MAX)
    {
        update_tcycles = std::max<uint32_t>(1, timer.top);
        if(timer.phase_correct)
            update_tcycles *= 2;
    }
    return std::max<uint32_t>(1, update_tcycles);
}

ARDENS_FORCEINLINE static void process_wgm16(
    uint32_t wgm, uint32_t& top, uint32_t& tov, uint32_t ocr, uint32_t icr)
{
    uint32_t ttop = 0xffff;
    uint32_t ttov = 0xffff;

    switch(wgm)
    {
    case 0x0: break;                    // normal
    case 0x1: ttop = 0x00ff; ttov = 0; break;
    case 0x2: ttop = 0x01ff; ttov = 0; break;
    case 0x3: ttop = 0x03ff; ttov = 0; break;
    case 0x4: ttop = ocr; break;        // CTC OCRnA
    case 0x5: ttop = 0x00ff; break;
    case 0x6: ttop = 0x01ff; break;
    case 0x7: ttop = 0x03ff; break;
    case 0x8: ttop = icr; ttov = 0; break;
    case 0x9: ttop = ocr; ttov = 0; break;
    case 0xa: ttop = icr; ttov = 0; break;
    case 0xb: ttop = ocr; ttov = 0; break;
    case 0xc: ttop = icr; break;        // CTC ICRn
    case 0xd: break;                    // reserved
    case 0xe: ttop = icr; break;
    case 0xf: ttop = std::max<uint32_t>(3, ocr); break;
    default: break;
    }

    top = ttop;
    tov = ttov;
}

ARDENS_FORCEINLINE static bool timer16_powered(
    atmega32u4_t const& cpu, atmega32u4_t::timer16_t const& timer)
{
    return (cpu.data[timer.prr_addr] & timer.prr_mask) == 0;
}

ARDENS_FORCEINLINE static bool timer16_running(
    atmega32u4_t const& cpu, atmega32u4_t::timer16_t const& timer)
{
    return timer16_powered(cpu, timer) && !sync_tsm(cpu) && timer.divider != 0;
}

static void timer16_refresh_config(
    atmega32u4_t& cpu, atmega32u4_t::timer16_t& timer, bool force_active)
{
    uint32_t old_divider = timer.divider;
    uint32_t addr = timer.base_addr;
    uint32_t tccrNa = cpu.data[addr + 0x0];
    uint32_t tccrNb = cpu.data[addr + 0x1];
    uint32_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);
    uint32_t wgm_mask = 1u << wgm;

    timer.divider = get_sync_divider(tccrNb & 0x7);
    timer.ocrNa_buffer = word(cpu, addr + 0x8);
    timer.ocrNb_buffer = word(cpu, addr + 0xa);
    timer.ocrNc_buffer = word(cpu, addr + 0xc);
    timer.icrN = word(cpu, addr + 0x6);
    timer.update_ocrN_at_bottom = (wgm_mask & 0x0300) != 0;
    timer.update_ocrN_at_top = (wgm_mask & 0xccee) != 0;
    timer.fast_pwm = (wgm_mask & 0xc0e0) != 0;
    timer.top_source_icr = (wgm_mask & 0x4500) != 0;
    timer.phase_correct = (wgm_mask & 0x0f0e) != 0;
    timer.com3a = tccrNa >> 6;

    bool immediate = (wgm_mask & 0x1011) != 0;
    bool stopped = !timer16_powered(cpu, timer) || timer.divider == 0;
    if(force_active || old_divider == 0 || stopped || immediate)
    {
        timer.ocrNa = timer.ocrNa_buffer;
        timer.ocrNb = timer.ocrNb_buffer;
        timer.ocrNc = timer.ocrNc_buffer;
    }

    if(timer.update_ocrN_at_bottom && timer.tcnt == 0)
    {
        timer.ocrNa = timer.ocrNa_buffer;
        timer.ocrNb = timer.ocrNb_buffer;
        timer.ocrNc = timer.ocrNc_buffer;
    }
    if(timer.update_ocrN_at_top && timer.tcnt == timer.top)
    {
        timer.ocrNa = timer.ocrNa_buffer;
        timer.ocrNb = timer.ocrNb_buffer;
        timer.ocrNc = timer.ocrNc_buffer;
    }

    process_wgm16(wgm, timer.top, timer.tov, timer.ocrNa, timer.icrN);
    if(!timer.phase_correct)
        timer.count_down = false;
}

ARDENS_FORCEINLINE static void toggle_portc6(atmega32u4_t& cpu)
{
    if(!(cpu.data[reg::addr::DDRC] & reg::bit::DDRC::DDC6)) return;
    cpu.update_sound();
    cpu.data[reg::addr::PINC] ^= reg::bit::PINC::PINC6;
}

ARDENS_FORCEINLINE static void clear_portc6(atmega32u4_t& cpu)
{
    if(!(cpu.data[reg::addr::DDRC] & reg::bit::DDRC::DDC6)) return;
    cpu.update_sound();
    cpu.data[reg::addr::PINC] &= ~reg::bit::PINC::PINC6;
}

ARDENS_FORCEINLINE static void set_portc6(atmega32u4_t& cpu)
{
    if(!(cpu.data[reg::addr::DDRC] & reg::bit::DDRC::DDC6)) return;
    cpu.update_sound();
    cpu.data[reg::addr::PINC] |= reg::bit::PINC::PINC6;
}

static void timer16_tick(
    atmega32u4_t& cpu,
    atmega32u4_t::timer16_t& timer,
    bool compare_allowed,
    uint8_t& tifr)
{
    uint32_t tcnt = timer.tcnt;

    if(timer.count_down)
    {
        if(tcnt != 0)
            --tcnt;
        if(tcnt == 0)
            tifr |= reg::bit::TIFR1::TOV1, timer.count_down = false;
    }
    else if(tcnt == timer.top)
    {
        if(timer.phase_correct)
        {
            timer.count_down = true;
            if(tcnt != 0)
                --tcnt;
            if(timer.top_source_icr)
                tifr |= reg::bit::TIFR1::ICF1;
        }
        else
        {
            if(timer.fast_pwm)
            {
                tifr |= reg::bit::TIFR1::TOV1;
                if(timer.top_source_icr)
                    tifr |= reg::bit::TIFR1::ICF1;
            }
            else if(timer.top == 0xffff)
                tifr |= reg::bit::TIFR1::TOV1;
            tcnt = 0;
        }
    }
    else if(tcnt > timer.top)
    {
        tcnt = (tcnt + 1) & 0xffff;
        if(timer.top == 0xffff && tcnt == 0)
            tifr |= reg::bit::TIFR1::TOV1;
    }
    else
    {
        ++tcnt;
    }

    timer.tcnt = tcnt & 0xffff;
    if(&timer == &cpu.timer3 && timer.tcnt == timer.ocrNa)
    {
        if(timer.com3a == 1) toggle_portc6(cpu);
        if(timer.com3a == 2) clear_portc6(cpu);
        if(timer.com3a == 3) set_portc6(cpu);
    }
    if(compare_allowed)
    {
        if(timer.tcnt == timer.ocrNa) tifr |= reg::bit::TIFR1::OCF1A;
        if(timer.tcnt == timer.ocrNb) tifr |= reg::bit::TIFR1::OCF1B;
        if(timer.tcnt == timer.ocrNc) tifr |= reg::bit::TIFR1::OCF1C;
    }
}

static void timer16_advance_ticks(
    atmega32u4_t& cpu, atmega32u4_t::timer16_t& timer, uint64_t ticks)
{
    uint8_t tifr = 0;

    if(timer.compare_block_next_tick && ticks != 0)
    {
        timer16_tick(cpu, timer, false, tifr);
        timer.compare_block_next_tick = false;
        --ticks;
    }

    uint32_t tcnt = timer.tcnt;
    bool count_down = timer.count_down;
    uint32_t ocrNa = timer.ocrNa;
    uint32_t ocrNb = timer.ocrNb;
    uint32_t ocrNc = timer.ocrNc;
    uint32_t top = timer.top;
    uint32_t com3a = timer.com3a;

    while(ticks > 0)
    {
        if(count_down)
        {
            uint32_t stop = 0;
            if(ocrNa < tcnt) stop = std::max(stop, ocrNa);
            if(ocrNb < tcnt) stop = std::max(stop, ocrNb);
            if(ocrNc < tcnt) stop = std::max(stop, ocrNc);
            uint64_t t = tcnt - stop;
            t = std::min(t, ticks);
            ticks -= t;
            tcnt -= uint32_t(t);
            if(tcnt == 0) tifr |= reg::bit::TIFR1::TOV1, count_down = false;
        }
        else if(tcnt == top)
        {
            if(timer.phase_correct)
            {
                count_down = true;
                --tcnt;
                if(timer.top_source_icr)
                    tifr |= reg::bit::TIFR1::ICF1;
            }
            else
            {
                if(timer.fast_pwm)
                {
                    tifr |= reg::bit::TIFR1::TOV1;
                    if(timer.top_source_icr)
                        tifr |= reg::bit::TIFR1::ICF1;
                }
                else if(top == 0xffff)
                    tifr |= reg::bit::TIFR1::TOV1;
                tcnt = 0;
            }
            ticks -= 1;
            if(timer.update_ocrN_at_top)
            {
                timer.ocrNa = timer.ocrNa_buffer;
                timer.ocrNb = timer.ocrNb_buffer;
                timer.ocrNc = timer.ocrNc_buffer;
                process_wgm16(
                    (cpu.data[timer.base_addr] & 0x3) |
                    ((cpu.data[timer.base_addr + 1] >> 1) & 0xc),
                    timer.top, timer.tov, timer.ocrNa, timer.icrN);
                top = timer.top;
                ocrNa = timer.ocrNa;
                ocrNb = timer.ocrNb;
                ocrNc = timer.ocrNc;
            }
        }
        else if(tcnt > top)
        {
            uint64_t t = 256;
            t = std::min(t, ticks);
            ticks -= t;
            tcnt += uint32_t(t);
            tcnt &= 0xffff;
            if(top == 0xffff)
                tifr |= reg::bit::TIFR1::TOV1;
        }
        else
        {
            uint32_t stop = top;
            if(ocrNa > tcnt) stop = std::min(stop, ocrNa);
            if(ocrNb > tcnt) stop = std::min(stop, ocrNb);
            if(ocrNc > tcnt) stop = std::min(stop, ocrNc);
            uint64_t t = stop - tcnt;
            t = std::min(t, ticks);
            ticks -= t;
            tcnt += uint32_t(t);
        }

        if(&timer == &cpu.timer3 && tcnt == ocrNa)
        {
            if(com3a == 1) toggle_portc6(cpu);
            if(com3a == 2) clear_portc6(cpu);
            if(com3a == 3) set_portc6(cpu);
        }
        if(tcnt == ocrNa) tifr |= reg::bit::TIFR1::OCF1A;
        if(tcnt == ocrNb) tifr |= reg::bit::TIFR1::OCF1B;
        if(tcnt == ocrNc) tifr |= reg::bit::TIFR1::OCF1C;
    }

    timer.tcnt = tcnt & 0xffff;
    timer.count_down = count_down;
    set_word(cpu, timer.base_addr + 0x4, timer.tcnt);
    set_timer_flags(cpu, timer.tifrN_addr, tifr);
}

static uint64_t timer16_next_ticks(
    atmega32u4_t const& cpu, atmega32u4_t::timer16_t const& timer)
{
    if(!timer16_running(cpu, timer))
        return UINT64_MAX;
    if(timer.compare_block_next_tick)
        return 1;

    uint32_t update_tcycles = UINT32_MAX;
    if(timer.count_down)
    {
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt - timer.ocrNa);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt - timer.ocrNb);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt - timer.ocrNc);
    }
    else
    {
        if(timer.phase_correct)
            update_tcycles = min_nonzero(update_tcycles, timer.top, timer.top - timer.tcnt);
        else
            update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tov - timer.tcnt + 1);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNa - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNb - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNc - timer.tcnt);
        if(timer.tcnt == timer.top && timer.top != 0)
            update_tcycles = 1;
    }
    if(update_tcycles == UINT32_MAX)
    {
        update_tcycles = std::max<uint32_t>(1, timer.top);
        if(timer.phase_correct)
            update_tcycles *= 2;
    }
    return std::max<uint32_t>(1, update_tcycles);
}

static void sync_refresh_configs(atmega32u4_t& cpu, bool force_active)
{
    timer0_refresh_config(cpu, force_active);
    timer16_refresh_config(cpu, cpu.timer1, force_active);
    timer16_refresh_config(cpu, cpu.timer3, force_active);
}

static void sync_reschedule(atmega32u4_t& cpu)
{
    uint64_t next = UINT64_MAX;
    uint32_t phase = cpu.timer_sync.prescaler_cycle;

    uint64_t ticks0 = timer0_next_ticks(cpu);
    cpu.timer0.next_update_cycle = UINT64_MAX;
    if(ticks0 != UINT64_MAX)
    {
        uint64_t cycles = cycles_until_sync_ticks(phase, cpu.timer0.divider, ticks0);
        cpu.timer0.next_update_cycle = cpu.timer_sync.prev_update_cycle + cycles;
        next = std::min(next, cpu.timer0.next_update_cycle);
    }

    uint64_t ticks1 = timer16_next_ticks(cpu, cpu.timer1);
    cpu.timer1.next_update_cycle = UINT64_MAX;
    if(ticks1 != UINT64_MAX)
    {
        uint64_t cycles = cycles_until_sync_ticks(phase, cpu.timer1.divider, ticks1);
        cpu.timer1.next_update_cycle = cpu.timer_sync.prev_update_cycle + cycles;
        next = std::min(next, cpu.timer1.next_update_cycle);
    }

    uint64_t ticks3 = timer16_next_ticks(cpu, cpu.timer3);
    cpu.timer3.next_update_cycle = UINT64_MAX;
    if(ticks3 != UINT64_MAX)
    {
        uint64_t cycles = cycles_until_sync_ticks(phase, cpu.timer3.divider, ticks3);
        cpu.timer3.next_update_cycle = cpu.timer_sync.prev_update_cycle + cycles;
        next = std::min(next, cpu.timer3.next_update_cycle);
    }

    cpu.timer_sync.next_update_cycle = next;
    cpu.peripheral_queue.reschedule(next, PQ_TIMER_SYNC);
}

static void sync_advance_to(atmega32u4_t& cpu, uint64_t target_cycle)
{
    if(target_cycle <= cpu.timer_sync.prev_update_cycle)
        return;

    uint64_t cycles = target_cycle - cpu.timer_sync.prev_update_cycle;
    uint32_t phase = cpu.timer_sync.prescaler_cycle;

    bool any_running =
        timer0_running(cpu) ||
        timer16_running(cpu, cpu.timer1) ||
        timer16_running(cpu, cpu.timer3);

    if(!sync_tsm(cpu) && any_running)
    {
        if(timer0_running(cpu))
            timer0_advance_ticks(
                cpu, ticks_from_sync_cycles(phase, cycles, cpu.timer0.divider));
        if(timer16_running(cpu, cpu.timer1))
            timer16_advance_ticks(
                cpu, cpu.timer1,
                ticks_from_sync_cycles(phase, cycles, cpu.timer1.divider));
        if(timer16_running(cpu, cpu.timer3))
            timer16_advance_ticks(
                cpu, cpu.timer3,
                ticks_from_sync_cycles(phase, cycles, cpu.timer3.divider));

        cpu.timer_sync.prescaler_cycle =
            uint32_t((phase + cycles) % SYNC_PRESCALER_PERIOD);
    }
    else if(sync_tsm(cpu))
    {
        cpu.timer_sync.prescaler_cycle = 0;
    }

    cpu.timer_sync.prev_update_cycle = target_cycle;
}

void atmega32u4_t::update_sync_timers()
{
    sync_advance_to(*this, cycle_count);
    sync_refresh_configs(*this, false);
    sync_reschedule(*this);
}

void atmega32u4_t::update_timer0()
{
    update_sync_timers();
}

ARDENS_FORCEINLINE void atmega32u4_t::update_timer1()
{
    update_sync_timers();
}

ARDENS_FORCEINLINE void atmega32u4_t::update_timer3()
{
    update_sync_timers();
}

ARDENS_FORCEINLINE static void sync_write_barrier(atmega32u4_t& cpu)
{
    sync_advance_to(cpu, cpu.cycle_count + 1);
}

void atmega32u4_t::timer0_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    sync_write_barrier(cpu);
    if(ptr == reg::addr::TCCR0B)
        x &= ~(reg::bit::TCCR0B::FOC0A | reg::bit::TCCR0B::FOC0B);
    cpu.data[ptr] = x;
    sync_refresh_configs(cpu, false);
    sync_reschedule(cpu);
}

void atmega32u4_t::timer0_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::TIFR0);
    cpu.data[reg::addr::TIFR0] &= ~x;
    cpu.schedule_interrupt_check();
}

uint8_t atmega32u4_t::timer0_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr)
{
    cpu.update_sync_timers();
    return cpu.data[ptr];
}

void atmega32u4_t::timer0_handle_st_tcnt(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::TCNT0);
    sync_write_barrier(cpu);
    cpu.data[ptr] = x;
    cpu.timer0.tcnt = x;
    cpu.timer0.compare_block_next_tick = true;
    sync_refresh_configs(cpu, false);
    sync_reschedule(cpu);
}

static void timer16_handle_st_regs(
    atmega32u4_t& cpu, atmega32u4_t::timer16_t& timer, uint16_t ptr, uint8_t x)
{
    sync_write_barrier(cpu);

    if(ptr >= timer.base_addr + 0x4 &&
        ptr <= timer.base_addr + 0xd &&
        (ptr & 1) == 1)
    {
        timer.temp = x;
        return;
    }

    if(ptr >= timer.base_addr + 0x4 &&
        ptr <= timer.base_addr + 0xd &&
        (ptr & 1) == 0)
    {
        cpu.data[ptr] = x;
        cpu.data[ptr + 1] = timer.temp;
        uint16_t val = (uint16_t(timer.temp) << 8) | x;
        if(ptr == timer.base_addr + 0x4)
        {
            timer.tcnt = val;
            timer.compare_block_next_tick = true;
        }
        sync_refresh_configs(cpu, false);
        sync_reschedule(cpu);
        return;
    }

    if(ptr == timer.base_addr + 0x2)
        x = 0; // force-compare bits are strobes and read back as zero

    cpu.data[ptr] = x;
    sync_refresh_configs(cpu, false);
    sync_reschedule(cpu);
}

void atmega32u4_t::timer1_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    timer16_handle_st_regs(cpu, cpu.timer1, ptr, x);
}

void atmega32u4_t::timer1_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::TIFR1);
    cpu.data[reg::addr::TIFR1] &= ~x;
    cpu.schedule_interrupt_check();
}

void atmega32u4_t::timer3_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    timer16_handle_st_regs(cpu, cpu.timer3, ptr, x);
}

void atmega32u4_t::timer3_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::TIFR3);
    cpu.data[reg::addr::TIFR3] &= ~x;
    cpu.schedule_interrupt_check();
}

static uint8_t timer16_handle_ld_reg16(
    atmega32u4_t& cpu, atmega32u4_t::timer16_t& timer, uint16_t ptr)
{
    cpu.update_sync_timers();
    if(ptr >= timer.base_addr + 0x4 &&
        ptr <= timer.base_addr + 0xd &&
        (ptr & 1) == 0)
    {
        timer.temp = cpu.data[ptr + 1];
        return cpu.data[ptr];
    }
    if(ptr >= timer.base_addr + 0x4 &&
        ptr <= timer.base_addr + 0xd &&
        (ptr & 1) == 1)
    {
        return timer.temp;
    }
    return cpu.data[ptr];
}

uint8_t atmega32u4_t::timer1_handle_ld_regs(atmega32u4_t& cpu, uint16_t ptr)
{
    return timer16_handle_ld_reg16(cpu, cpu.timer1, ptr);
}

uint8_t atmega32u4_t::timer3_handle_ld_regs(atmega32u4_t& cpu, uint16_t ptr)
{
    return timer16_handle_ld_reg16(cpu, cpu.timer3, ptr);
}

ARDENS_FORCEINLINE static void timer10_update_ocrN(
    atmega32u4_t& cpu,
    atmega32u4_t::timer10_t& timer)
{
    if(!timer.tlock)
    {
        timer.ocrNa = timer.ocrNa_next;
        timer.ocrNb = timer.ocrNb_next;
        timer.ocrNc = timer.ocrNc_next;
        timer.ocrNd = timer.ocrNd_next;

        cpu.data[reg::addr::OCR4A] = uint8_t(timer.ocrNa);
        cpu.data[reg::addr::OCR4B] = uint8_t(timer.ocrNb);
        cpu.data[reg::addr::OCR4C] = uint8_t(timer.ocrNc);
        cpu.data[reg::addr::OCR4D] = uint8_t(timer.ocrNd);
    }
}

ARDENS_FORCEINLINE static bool timer4_powered(atmega32u4_t const& cpu)
{
    return (cpu.data[reg::addr::PRR1] & reg::bit::PRR1::PRTIM4) == 0;
}

ARDENS_FORCEINLINE static bool timer4_running(atmega32u4_t const& cpu)
{
    return timer4_powered(cpu) && cpu.timer4.divider != 0;
}

ARDENS_FORCEINLINE static void set_portc(atmega32u4_t& cpu, bool c6, bool c7)
{
    uint8_t c = 0;
    if(c6) c |= reg::bit::PINC::PINC6;
    if(c7) c |= reg::bit::PINC::PINC7;
    uint8_t m = cpu.data[reg::addr::DDRC];
    c &= m;
    uint8_t t = cpu.data[reg::addr::PINC];
    t &= ~m;
    t |= c;
    cpu.update_sound();
    cpu.data[reg::addr::PINC] = t;
}

ARDENS_FORCEINLINE static void set_portc6(atmega32u4_t& cpu, bool c6)
{
    uint8_t c = 0;
    if(c6) c |= reg::bit::PINC::PINC6;
    if(!(cpu.data[reg::addr::DDRC] & reg::bit::DDRC::DDC6)) return;
    c &= reg::bit::PINC::PINC6;
    uint8_t t = cpu.data[reg::addr::PINC];
    t &= ~reg::bit::PINC::PINC6;
    t |= c;
    cpu.update_sound();
    cpu.data[reg::addr::PINC] = t;
}

ARDENS_FORCEINLINE static void set_portc7(atmega32u4_t& cpu, bool c7)
{
    uint8_t c = 0;
    if(c7) c |= reg::bit::PINC::PINC7;
    if(!(cpu.data[reg::addr::DDRC] & reg::bit::DDRC::DDC7)) return;
    c &= reg::bit::PINC::PINC7;
    uint8_t t = cpu.data[reg::addr::PINC];
    t &= ~reg::bit::PINC::PINC7;
    t |= c;
    cpu.update_sound();
    cpu.data[reg::addr::PINC] = t;
}

static void timer4_refresh_config(atmega32u4_t& cpu, bool force_active)
{
    auto& timer = cpu.timer4;
    uint32_t old_divider = timer.divider;
    uint32_t tccr4a = cpu.data[reg::addr::TCCR4A];
    uint32_t tccr4b = cpu.data[reg::addr::TCCR4B];
    uint32_t tccr4c = cpu.data[reg::addr::TCCR4C];
    uint32_t tccr4d = cpu.data[reg::addr::TCCR4D];
    uint32_t tccr4e = cpu.data[reg::addr::TCCR4E];

    bool old_tlock = timer.tlock;
    timer.tlock = (tccr4e & reg::bit::TCCR4E::TLOCK4) != 0;
    timer.enhc = (tccr4e & reg::bit::TCCR4E::ENHC4) != 0;

    uint32_t cs = tccr4b & 0xf;
    timer.divider = (cs == 0 ? 0 : 1u << (cs - 1));

    bool channel_pwm_enabled =
        (tccr4a & (reg::bit::TCCR4A::PWM4A | reg::bit::TCCR4A::PWM4B)) != 0 ||
        (tccr4c & reg::bit::TCCR4C::PWM4D) != 0;
    uint32_t wgm = tccr4d & 0x3;

    // Timer4's compare registers are commonly used as a synchronized PWM group
    // even when only OC4A sound output is externally observable here.
    bool buffered_pwm_updates = true;
    timer.update_ocrN_at_top = buffered_pwm_updates && ((wgm & 0x1) == 0);
    timer.update_ocrN_at_bottom = buffered_pwm_updates && ((wgm & 0x1) != 0);
    timer.phase_correct = channel_pwm_enabled && ((wgm & 0x1) != 0);
    if(!timer.phase_correct)
        timer.count_down = false;

    bool stopped = !timer4_powered(cpu) || timer.divider == 0;
    if(force_active || stopped || old_divider == 0 || !buffered_pwm_updates || (old_tlock && !timer.tlock))
        timer10_update_ocrN(cpu, timer);
    if(timer.update_ocrN_at_bottom && timer.tcnt == 0)
        timer10_update_ocrN(cpu, timer);
    if(timer.update_ocrN_at_top && timer.tcnt == timer.top)
        timer10_update_ocrN(cpu, timer);

    timer.top = std::max<uint32_t>(3, timer.ocrNc);
    timer.tov = timer.top;

    if(stopped)
    {
        timer.divider_cycle = 0;
        timer.source_cycle = 0;
        timer.next_update_cycle = UINT64_MAX;
        cpu.sound_pwm = false;
        return;
    }

    if(old_divider == 0 && timer.divider == 1)
        timer.start_delay_cycles = std::max<uint32_t>(timer.start_delay_cycles, 3);

    cpu.update_sound();
    cpu.sound_pwm = false;
    uint32_t com = timer.com4a = tccr4a >> 6;
    if((tccr4a & (reg::bit::TCCR4A::COM4A1 | reg::bit::TCCR4A::COM4A0)) != 0 &&
        (tccr4a & reg::bit::TCCR4A::PWM4A) != 0)
    {
        uint32_t period = (timer.top + 1) * timer.divider;
        if(wgm != 0) period *= 2;
        if(timer.enhc) period = std::max<uint32_t>(1, period / 2);

        int32_t val = timer.ocrNa_next * atmega32u4_t::SOUND_GAIN /
            std::max<uint32_t>(1, timer.top);
        if(timer.enhc) val /= 2;
        if(com == 1) val = val * 2 - atmega32u4_t::SOUND_GAIN;
        if(com == 3) val = atmega32u4_t::SOUND_GAIN - val;
        cpu.sound_pwm_val = (int16_t)val;

        uint32_t max_period = 800; // 20kHz
        if(cpu.pll_num12 > 0)
            period *= 12, max_period *= cpu.pll_num12;
        if(period <= max_period)
            cpu.sound_pwm = true;
    }
}

ARDENS_FORCEINLINE static uint64_t timer4_source_ticks(
    atmega32u4_t& cpu, uint64_t cycles)
{
    if(cpu.pll_num12 == 0)
        return cycles;
    uint64_t t = cycles * cpu.pll_num12 + cpu.timer4.source_cycle;
    uint64_t d = t / 12;
    cpu.timer4.source_cycle = uint32_t(t % 12);
    return d;
}

ARDENS_FORCEINLINE static uint64_t source_cycles_until_ticks(
    atmega32u4_t const& cpu, uint64_t source_ticks)
{
    if(source_ticks == 0)
        return 0;
    if(cpu.pll_num12 == 0)
        return source_ticks;
    uint64_t need = source_ticks * 12;
    uint64_t have = cpu.timer4.source_cycle;
    if(need <= have)
        return 1;
    return (need - have + cpu.pll_num12 - 1) / cpu.pll_num12;
}

ARDENS_FORCEINLINE static uint64_t ticks_from_timer4_source(
    uint32_t divider_phase, uint64_t source_ticks, uint32_t divider)
{
    if(divider == 0)
        return 0;
    if(divider == 1)
        return source_ticks;
    return (divider_phase + source_ticks) / divider;
}

ARDENS_FORCEINLINE static uint64_t source_until_timer4_ticks(
    uint32_t divider_phase, uint32_t divider, uint64_t ticks)
{
    if(ticks == 0 || divider == 0)
        return UINT64_MAX;
    if(divider == 1)
        return ticks;
    uint64_t first = divider - divider_phase;
    if(first == 0)
        first = divider;
    return first + (ticks - 1) * divider;
}

static void timer4_tick(atmega32u4_t& cpu, bool compare_allowed, uint8_t& tifr)
{
    auto& timer = cpu.timer4;
    uint32_t tcnt = timer.tcnt;
    uint32_t ocrNa = timer.enhc ? timer.ocrNa / 2 : timer.ocrNa;
    uint32_t ocrNb = timer.enhc ? timer.ocrNb / 2 : timer.ocrNb;
    uint32_t ocrNd = timer.enhc ? timer.ocrNd / 2 : timer.ocrNd;

    if(compare_allowed)
    {
        if(tcnt == ocrNa) tifr |= reg::bit::TIFR4::OCF4A;
        if(tcnt == ocrNb) tifr |= reg::bit::TIFR4::OCF4B;
        if(tcnt == ocrNd) tifr |= reg::bit::TIFR4::OCF4D;
    }

    if(timer.count_down)
    {
        if(tcnt != 0)
            --tcnt;
        if(tcnt == 0)
            tifr |= reg::bit::TIFR4::TOV4, timer.count_down = false;
    }
    else if(tcnt == timer.top)
    {
        if(timer.phase_correct)
            timer.count_down = true, --tcnt;
        else
            tifr |= reg::bit::TIFR4::TOV4, tcnt = 0;
        if(timer.update_ocrN_at_top)
            timer10_update_ocrN(cpu, timer);
        if(timer.com4a == 1) set_portc(cpu, true, false);
        if(timer.com4a == 2) set_portc7(cpu, true);
        if(timer.com4a == 3) set_portc7(cpu, false);
    }
    else if(tcnt > timer.top)
    {
        tcnt = (tcnt + 1) & 0x07ff;
        if(tcnt < 0x800)
            tifr |= reg::bit::TIFR4::TOV4;
    }
    else
    {
        ++tcnt;
    }

    timer.tcnt = tcnt & 0x07ff;
    if(compare_allowed && timer.tcnt == ocrNa)
    {
        if(timer.com4a == 1) set_portc(cpu, false, true);
        if(timer.com4a == 2) set_portc7(cpu, false);
        if(timer.com4a == 3) set_portc7(cpu, true);
    }
}

static void timer4_advance_ticks(atmega32u4_t& cpu, uint64_t ticks)
{
    auto& timer = cpu.timer4;
    uint8_t tifr = 0;

    if(timer.compare_block_next_tick && ticks != 0)
    {
        timer4_tick(cpu, false, tifr);
        timer.compare_block_next_tick = false;
        --ticks;
    }

    uint32_t tcnt = timer.tcnt;
    bool count_down = timer.count_down;
    uint32_t ocrNa = timer.enhc ? timer.ocrNa / 2 : timer.ocrNa;
    uint32_t ocrNb = timer.enhc ? timer.ocrNb / 2 : timer.ocrNb;
    uint32_t ocrNd = timer.enhc ? timer.ocrNd / 2 : timer.ocrNd;
    uint32_t top = timer.top;
    uint32_t com4a = timer.com4a;

    while(ticks > 0)
    {
        if(tcnt == ocrNa) tifr |= reg::bit::TIFR4::OCF4A;
        if(tcnt == ocrNb) tifr |= reg::bit::TIFR4::OCF4B;
        if(tcnt == ocrNd) tifr |= reg::bit::TIFR4::OCF4D;

        if(count_down)
        {
            uint32_t stop = 0;
            if(ocrNa < tcnt) stop = std::max(stop, ocrNa);
            if(ocrNb < tcnt) stop = std::max(stop, ocrNb);
            if(ocrNd < tcnt) stop = std::max(stop, ocrNd);
            uint64_t t = tcnt - stop;
            t = std::min(t, ticks);
            ticks -= t;
            tcnt -= uint32_t(t);
            if(tcnt == 0) tifr |= reg::bit::TIFR4::TOV4, count_down = false;
        }
        else if(tcnt == top)
        {
            if(timer.phase_correct)
                count_down = true, --tcnt;
            else
                tifr |= reg::bit::TIFR4::TOV4, tcnt = 0;
            ticks -= 1;
            if(timer.update_ocrN_at_top)
            {
                timer10_update_ocrN(cpu, timer);
                top = timer.top;
                ocrNa = timer.enhc ? timer.ocrNa / 2 : timer.ocrNa;
                ocrNb = timer.enhc ? timer.ocrNb / 2 : timer.ocrNb;
                ocrNd = timer.enhc ? timer.ocrNd / 2 : timer.ocrNd;
            }
            if(com4a == 1) set_portc(cpu, true, false);
            if(com4a == 2) set_portc7(cpu, true);
            if(com4a == 3) set_portc7(cpu, false);
        }
        else if(tcnt > top)
        {
            uint64_t t = 256;
            t = std::min(t, ticks);
            ticks -= t;
            tcnt += uint32_t(t);
            tcnt &= 0x07ff;
            if(tcnt < 0x800) tifr |= reg::bit::TIFR4::TOV4;
        }
        else
        {
            uint32_t stop = top;
            if(ocrNa > tcnt) stop = std::min(stop, ocrNa);
            if(ocrNb > tcnt) stop = std::min(stop, ocrNb);
            if(ocrNd > tcnt) stop = std::min(stop, ocrNd);
            uint64_t t = stop - tcnt;
            t = std::min(t, ticks);
            ticks -= t;
            tcnt += uint32_t(t);
        }

        if(tcnt == ocrNa)
        {
            if(com4a == 1) set_portc(cpu, false, true);
            if(com4a == 2) set_portc7(cpu, false);
            if(com4a == 3) set_portc7(cpu, true);
        }
    }

    timer.tcnt = tcnt;
    timer.count_down = count_down;

    cpu.data[reg::addr::TCNT4] = uint8_t(timer.tcnt);
    set_timer_flags(cpu, reg::addr::TIFR4, tifr);
}

static uint64_t timer4_next_ticks(atmega32u4_t const& cpu)
{
    auto const& timer = cpu.timer4;
    if(!timer4_running(cpu))
        return UINT64_MAX;
    if(timer.compare_block_next_tick)
        return 1;

    uint32_t ocrNa = timer.enhc ? timer.ocrNa / 2 : timer.ocrNa;
    uint32_t ocrNb = timer.enhc ? timer.ocrNb / 2 : timer.ocrNb;
    uint32_t ocrNd = timer.enhc ? timer.ocrNd / 2 : timer.ocrNd;
    uint32_t update_tcycles = UINT32_MAX;

    if(timer.count_down)
    {
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt - ocrNa);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt - ocrNb);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tcnt - ocrNd);
    }
    else
    {
        if(timer.phase_correct)
            update_tcycles = min_nonzero(update_tcycles, timer.top, timer.top - timer.tcnt);
        else
            update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tov - timer.tcnt + 1);
        update_tcycles = min_nonzero(update_tcycles, timer.top, ocrNa - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, ocrNb - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, ocrNd - timer.tcnt);
        if(timer.tcnt == timer.top && timer.top != 0)
            update_tcycles = 1;
    }
    if(update_tcycles == UINT32_MAX)
    {
        update_tcycles = std::max<uint32_t>(1, timer.top);
        if(timer.phase_correct)
            update_tcycles *= 2;
    }
    return std::max<uint32_t>(1, update_tcycles);
}

static void timer4_reschedule(atmega32u4_t& cpu)
{
    auto& timer = cpu.timer4;
    uint64_t next = UINT64_MAX;
    if(timer.start_delay_cycles != 0)
    {
        next = timer.prev_update_cycle + timer.start_delay_cycles;
    }
    else if(timer4_running(cpu))
    {
        uint64_t timer_ticks = timer4_next_ticks(cpu);
        uint64_t source_ticks = source_until_timer4_ticks(
            timer.divider_cycle, timer.divider, timer_ticks);
        uint64_t cycles = source_cycles_until_ticks(cpu, source_ticks);
        if(cycles != UINT64_MAX)
            next = timer.prev_update_cycle + std::max<uint64_t>(1, cycles);
    }
    timer.next_update_cycle = next;
    cpu.peripheral_queue.reschedule(next, PQ_TIMER4);
}

static void timer4_advance_to(atmega32u4_t& cpu, uint64_t target_cycle)
{
    auto& timer = cpu.timer4;
    if(target_cycle <= timer.prev_update_cycle)
        return;

    uint64_t cycles = target_cycle - timer.prev_update_cycle;
    if(timer.start_delay_cycles != 0)
    {
        if(cycles < timer.start_delay_cycles)
        {
            timer.start_delay_cycles -= uint32_t(cycles);
            timer.prev_update_cycle = target_cycle;
            return;
        }
        cycles -= timer.start_delay_cycles;
        timer.start_delay_cycles = 0;
    }

    if(timer4_running(cpu) && cycles != 0)
    {
        uint64_t source_ticks = timer4_source_ticks(cpu, cycles);
        uint64_t timer_ticks = ticks_from_timer4_source(
            timer.divider_cycle, source_ticks, timer.divider);
        if(timer.divider != 0)
            timer.divider_cycle = uint32_t(
                (timer.divider_cycle + source_ticks) % timer.divider);
        timer4_advance_ticks(cpu, timer_ticks);
    }

    timer.prev_update_cycle = target_cycle;
}

void atmega32u4_t::update_timer4()
{
    timer4_advance_to(*this, cycle_count);
    timer4_refresh_config(*this, false);
    timer4_reschedule(*this);
}

ARDENS_FORCEINLINE static void timer4_write_barrier(atmega32u4_t& cpu)
{
    timer4_advance_to(cpu, cpu.cycle_count + 1);
}

void atmega32u4_t::timer4_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    timer4_write_barrier(cpu);

    if(ptr == reg::addr::TIFR4)
        x = cpu.data[reg::addr::TIFR4] & ~x;

    if(ptr == reg::addr::TCCR4B)
    {
        if(x & reg::bit::TCCR4B::PSR4)
            cpu.timer4.divider_cycle = 0;
        x &= ~reg::bit::TCCR4B::PSR4;
    }

    if(ptr == reg::addr::TC4H)
        x &= 0x07;

    cpu.data[ptr] = x;

    if(ptr == reg::addr::TCNT4)
    {
        uint32_t hi_mask = cpu.timer4.enhc ? 0x07 : 0x03;
        uint32_t tcnt = (((uint32_t)cpu.data[reg::addr::TC4H] & hi_mask) << 8) | x;
        tcnt &= 0x07ff;
        cpu.timer4.compare_block_next_tick = true;
        if(!timer4_running(cpu))
        {
            cpu.timer4.tcnt = tcnt;
            cpu.data[reg::addr::TCNT4] = uint8_t(tcnt);
            cpu.timer4.tcnt_write_pending = false;
            cpu.timer4.tcnt_write_pending_seen = false;
        }
        else
        {
            cpu.timer4.tcnt_write_pending = true;
            cpu.timer4.tcnt_write_pending_seen = false;
            cpu.timer4.tcnt_write_value = tcnt;
            cpu.data[reg::addr::TCNT4] = uint8_t(cpu.timer4.tcnt);
        }
    }

    timer4_refresh_config(cpu, false);
    timer4_reschedule(cpu);
}

void atmega32u4_t::timer4_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::TIFR4);
    cpu.data[reg::addr::TIFR4] &= ~x;
    cpu.schedule_interrupt_check();
}

void atmega32u4_t::timer4_handle_st_ocrN(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    timer4_write_barrier(cpu);
    uint16_t ocr = ((uint16_t)cpu.data[reg::addr::TC4H] << 8);
    ocr &= (cpu.timer4.enhc ? 0x700 : 0x300);
    ocr |= x;
    if(ptr == reg::addr::OCR4A) cpu.timer4.ocrNa_next = ocr;
    if(ptr == reg::addr::OCR4B) cpu.timer4.ocrNb_next = ocr;
    if(ptr == reg::addr::OCR4C) cpu.timer4.ocrNc_next = ocr;
    if(ptr == reg::addr::OCR4D) cpu.timer4.ocrNd_next = ocr;
    cpu.data[ptr] = x;
    timer4_refresh_config(cpu, false);
    timer4_reschedule(cpu);
}

uint8_t atmega32u4_t::timer4_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr)
{
    if(cpu.timer4.tcnt_write_pending && cpu.timer4.tcnt_write_pending_seen)
    {
        cpu.timer4.prev_update_cycle = cpu.cycle_count;
        cpu.timer4.tcnt = cpu.timer4.tcnt_write_value;
        cpu.timer4.compare_block_next_tick = true;
        cpu.timer4.tcnt_write_pending = false;
        cpu.timer4.tcnt_write_pending_seen = false;
        cpu.data[reg::addr::TCNT4] = uint8_t(cpu.timer4.tcnt);
    }

    cpu.update_timer4();
    if(ptr == reg::addr::TCNT4)
    {
        cpu.timer4.tc4h_latch = uint8_t((cpu.timer4.tcnt >> 8) & 0x07);
        cpu.data[reg::addr::TC4H] = cpu.timer4.tc4h_latch;
    }
    if(cpu.timer4.tcnt_write_pending)
        cpu.timer4.tcnt_write_pending_seen = true;
    return cpu.data[ptr];
}

void atmega32u4_t::st_handle_gtccr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::GTCCR);
    sync_write_barrier(cpu);
    if(x & (reg::bit::GTCCR::PSRSYNC | reg::bit::GTCCR::TSM))
        cpu.timer_sync.prescaler_cycle = 0;
    cpu.data[reg::addr::GTCCR] = x & reg::bit::GTCCR::TSM;
    sync_refresh_configs(cpu, false);
    sync_reschedule(cpu);
}

}
