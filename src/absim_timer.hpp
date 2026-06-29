#include "absim.hpp"

namespace absim
{

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

ARDENS_FORCEINLINE static uint32_t get_divider(uint32_t cs)
{
    assert((cs & ~7) == 0);
    static constexpr uint32_t DIVIDERS[8] =
    {
        0, 1, 8, 64, 256, 1024, 0, 0,
    };
    return DIVIDERS[cs];
}

ARDENS_FORCEINLINE static uint32_t min_nonzero(uint32_t a, uint32_t top, int32_t b)
{
    if(b <= 0) return std::min(a, top);
    return std::min(a, (uint32_t)b);
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

ARDENS_FORCEINLINE static void timer8_update_ocrN(
    atmega32u4_t& cpu,
    atmega32u4_t::timer8_t& timer)
{
    timer.ocrNa = cpu.data[reg::addr::OCR0A];
    timer.ocrNb = cpu.data[reg::addr::OCR0B];

    uint32_t tccr0a = cpu.data[reg::addr::TCCR0A];
    uint32_t tccr0b = cpu.data[reg::addr::TCCR0B];
    uint32_t wgm = (tccr0a & 0x3) | ((tccr0b >> 1) & 0x4);
    process_wgm8(wgm, timer.top, timer.tov, timer.ocrNa);
}

ARDENS_FORCEINLINE static void update_timer8_state(
    atmega32u4_t& cpu,
    atmega32u4_t::timer8_t& timer,
    uint64_t cycles)
{
    // find out how many timer cycles happened after prescaler
    uint32_t timer_cycles = increase_counter(
        timer.prescaler_cycle, (uint32_t)cycles, timer.divider);

    auto tcnt = timer.tcnt;
    bool count_down = timer.count_down;
    bool phase_correct = timer.phase_correct;
    auto ocrNa = timer.ocrNa;
    auto ocrNb = timer.ocrNb;
    auto tov = timer.tov;
    auto top = timer.top;
    uint8_t tifr = 0;

    while(timer_cycles > 0)
    {
        if(count_down)
        {
            uint32_t stop = 0;
            if(ocrNa < tcnt) stop = std::max(stop, ocrNa);
            if(ocrNb < tcnt) stop = std::max(stop, ocrNb);
            uint32_t t = tcnt - stop;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt -= t;
        if(tcnt == 0)
            tifr |= reg::bit::TIFR0::TOV0, count_down = false;
        }
        else if(tcnt > top)
        {
            uint32_t t = 256;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt += t;
            tcnt &= 0xff;
            if(top == 0xff)
                tifr |= reg::bit::TIFR0::TOV0;
        }
        else
        {
            uint32_t stop = top + 1;
            if(ocrNa > tcnt) stop = std::min(stop, ocrNa);
            if(ocrNb > tcnt) stop = std::min(stop, ocrNb);
            uint32_t t = stop - tcnt;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt += t;
            if(tcnt == top + 1)
            {
                if(phase_correct)
                    count_down = true, tcnt -= 2;
                else
                {
                    if(top == 0xff)
                        tifr |= reg::bit::TIFR0::TOV0;
                    tcnt = 0;
                }
            }
        }
        if(tcnt == ocrNa) tifr |= reg::bit::TIFR0::OCF0A;
        if(tcnt == ocrNb) tifr |= reg::bit::TIFR0::OCF0B;
    }

    timer.tcnt = tcnt;
    timer.count_down = count_down;
    if(tifr)
        cpu.schedule_interrupt_check();
    cpu.data[reg::addr::TIFR0] |= tifr;
    cpu.data[reg::addr::TCNT0] = uint8_t(tcnt);
}

void atmega32u4_t::update_timer0()
{
    uint32_t old_divider = timer0.divider;

    // first compute what happened to tcnt/tifr during the cycles
    if(!(timer0.divider == 0 || (data[reg::addr::PRR0] & reg::bit::PRR0::PRTIM0)))
    {
        // timer clock is running and timer is not powered down...
        uint64_t cycles = cycle_count - timer0.prev_update_cycle;
        update_timer8_state(*this, timer0, cycles);
    }
    timer0.prev_update_cycle = cycle_count;

    // now set up timer state for next update

    uint32_t cs = data[reg::addr::TCCR0B] &
        (reg::bit::TCCR0B::CS00 | reg::bit::TCCR0B::CS01 | reg::bit::TCCR0B::CS02);
    timer0.divider = get_divider(cs);

    if(timer0.divider == 0 || (data[reg::addr::PRR0] & reg::bit::PRR0::PRTIM0))
    {
        timer0.prescaler_cycle = 0;
        timer0.next_update_cycle = UINT64_MAX;
        return;
    }

    uint32_t tccr0a = data[reg::addr::TCCR0A];
    uint32_t tccr0b = data[reg::addr::TCCR0B];
    uint32_t wgm = (tccr0a & 0x3) | ((tccr0b >> 1) & 0x4);

    uint32_t wgm_mask = 1 << wgm;
    if(old_divider == 0 || (wgm_mask & 0x5)) // update ocrN immediately
        timer8_update_ocrN(*this, timer0);
    timer0.update_ocrN_at_top = ((wgm_mask & 0xaa) != 0);

    if(timer0.update_ocrN_at_top && timer0.tcnt == timer0.top)
        timer8_update_ocrN(*this, timer0);

    process_wgm8(wgm, timer0.top, timer0.tov, timer0.ocrNa);
    timer0.phase_correct = (wgm_mask & 0x22) != 0;
    if(!timer0.phase_correct)
        timer0.count_down = false;

    // compute next update cycle

    uint32_t update_tcycles = UINT32_MAX;
    if(timer0.count_down)
    {
        update_tcycles = min_nonzero(update_tcycles, timer0.top, timer0.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer0.top, timer0.tcnt - timer0.ocrNa);
        update_tcycles = min_nonzero(update_tcycles, timer0.top, timer0.tcnt - timer0.ocrNb);
    }
    else
    {
        if(timer0.phase_correct)
            update_tcycles = min_nonzero(update_tcycles, timer0.top, timer0.top - timer0.tcnt);
        else
            update_tcycles = min_nonzero(update_tcycles, timer0.top, timer0.tov - timer0.tcnt + 1);
        update_tcycles = min_nonzero(update_tcycles, timer0.top, timer0.ocrNa - timer0.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer0.top, timer0.ocrNb - timer0.tcnt);
        if(timer0.tcnt == timer0.top && timer0.top != 0)
            update_tcycles = 1;
    }
    if(update_tcycles == UINT32_MAX)
    {
        // all ocrN's and tov are the same. set to period
        update_tcycles = timer0.top;
        if(timer0.phase_correct)
            update_tcycles *= 2;
    }

    if(update_tcycles == 0)
        timer0.next_update_cycle = UINT64_MAX;
    else
    {
        update_tcycles = std::max<uint32_t>(1, update_tcycles);
        uint64_t update_cycles = (uint64_t)update_tcycles * timer0.divider - timer0.prescaler_cycle;
        timer0.next_update_cycle = cycle_count + update_cycles;
    }

    peripheral_queue.schedule(timer0.next_update_cycle, PQ_TIMER0);
}

ARDENS_FORCEINLINE static void process_wgm16(
    uint32_t wgm, uint32_t& top, uint32_t& tov, uint32_t ocr, uint32_t icr)
{
    uint32_t ttop = 0xffff;
    uint32_t ttov = 0xffff;

    switch(wgm)
    {
    case 0x0: // normal
        break;
    case 0x1: // PWM, Phase Correct, 8-bit
        ttop = 0x00ff;
        ttov = 0x0000;
        break;
    case 0x2: // PWM, Phase Correct, 9-bit
        ttop = 0x01ff;
        ttov = 0x0000;
        break;
    case 0x3: // PWM, Phase Correct, 10-bit
        ttop = 0x03ff;
        ttov = 0x0000;
        break;
    case 0x4: // CTC
        ttop = ocr;
        break;
    case 0x5: // Fast PWM, 8-bit
        ttop = 0x00ff;
        break;
    case 0x6: // Fast PWM, 9-bit
        ttop = 0x01ff;
        break;
    case 0x7: // Fast PWM, 10-bit
        ttop = 0x03ff;
        break;
    case 0x8: // PWM, Phase and Frequency Correct
        ttop = icr;
        ttov = 0x0000;
        break;
    case 0x9: // PWM, Phase and Frequency Correct
        ttop = ocr;
        ttov = 0x0000;
        break;
    case 0xa: // PWM, Phase Correct
        ttop = icr;
        ttov = 0x0000;
        break;
    case 0xb: // PWM, Phase Correct
        ttop = ocr;
        ttov = 0x0000;
        break;
    case 0xc: // CTC
        ttop = icr;
        break;
    case 0xd: // (Reserved)
        break;
    case 0xe: // Fast PWM
        ttop = icr;
        break;
    case 0xf: // Fast PWM
        ttop = ocr;
        break;
    default:
        break;
    }

    top = ttop;
    if(wgm == 0xf)
        top = std::max<uint32_t>(3, top);
    tov = ttov;
}

ARDENS_FORCEINLINE static void timer16_update_ocrN(
    atmega32u4_t& cpu,
    atmega32u4_t::timer16_t& timer,
    uint32_t addr)
{
    timer.ocrNa = word(cpu, addr + 0x8);
    timer.ocrNb = word(cpu, addr + 0xa);
    timer.ocrNc = word(cpu, addr + 0xc);

    uint32_t icrN = word(cpu, addr + 0x6);
    uint32_t tccrNa = cpu.data[addr + 0x0];
    uint32_t tccrNb = cpu.data[addr + 0x1];
    uint32_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);

    process_wgm16(wgm, timer.top, timer.tov, timer.ocrNa, icrN);
}

ARDENS_FORCEINLINE static uint32_t timer16_period(
    atmega32u4_t const& cpu,
    atmega32u4_t::timer16_t const& timer)
{
    uint32_t period = UINT32_MAX;
    uint32_t timsk = cpu.data[timer.timskN_addr];
    if(timsk & reg::bit::TIMSK1::TOIE1) period = std::min(period, timer.tov);
    if(timsk & reg::bit::TIMSK1::OCIE1A) period = std::min(period, timer.ocrNa);
    if(timsk & reg::bit::TIMSK1::OCIE1B) period = std::min(period, timer.ocrNb);
    if(timsk & reg::bit::TIMSK1::OCIE1C) period = std::min(period, timer.ocrNc);
    return period;
}

ARDENS_FORCEINLINE static void toggle_portc6(atmega32u4_t& cpu)
{
    if(!(cpu.data[reg::addr::DDRC] & reg::bit::DDRC::DDC6)) return;
    cpu.update_sound();
    cpu.data[reg::addr::PINC] ^= (1 << 6);
}

ARDENS_FORCEINLINE static void clear_portc6(atmega32u4_t& cpu)
{
    if(!(cpu.data[reg::addr::DDRC] & reg::bit::DDRC::DDC6)) return;
    cpu.update_sound();
    cpu.data[reg::addr::PINC] &= ~(1 << 6);
}

ARDENS_FORCEINLINE static void set_portc6(atmega32u4_t& cpu)
{
    if(!(cpu.data[reg::addr::DDRC] & reg::bit::DDRC::DDC6)) return;
    cpu.update_sound();
    cpu.data[reg::addr::PINC] |= (1 << 6);
}

ARDENS_FORCEINLINE static void update_timer16_state(
    atmega32u4_t& cpu,
    atmega32u4_t::timer16_t& timer,
    uint64_t cycles)
{
    // find out how many timer cycles happened after prescaler
    uint32_t timer_cycles = increase_counter(
        timer.prescaler_cycle, (uint32_t)cycles, timer.divider);

    auto tcnt = timer.tcnt;
    bool count_down = timer.count_down;
    bool phase_correct = timer.phase_correct;
    auto ocrNa = timer.ocrNa;
    auto ocrNb = timer.ocrNb;
    auto ocrNc = timer.ocrNc;
    auto tov = timer.tov;
    auto top = timer.top;
    auto com3a = timer.com3a;
    uint8_t tifr = 0;

    while(timer_cycles > 0)
    {
        if(count_down)
        {
            uint32_t stop = 0;
            if(ocrNa < tcnt) stop = std::max(stop, ocrNa);
            if(ocrNb < tcnt) stop = std::max(stop, ocrNb);
            if(ocrNc < tcnt) stop = std::max(stop, ocrNc);
            uint32_t t = tcnt - stop;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt -= t;
            if(tcnt == 0) tifr |= reg::bit::TIFR1::TOV1, count_down = false;
        }
        else if(tcnt == top)
        {
            if(phase_correct)
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
            timer_cycles -= 1;
            if(timer.update_ocrN_at_top)
            {
                timer16_update_ocrN(cpu, timer, timer.base_addr);
                top = timer.top;
                tov = timer.tov;
                ocrNa = timer.ocrNa;
                ocrNb = timer.ocrNb;
                ocrNc = timer.ocrNc;
            }
        }
        else if(tcnt > top)
        {
            uint32_t t = 256;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt += t;
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
            uint32_t t = stop - tcnt;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt += t;
        }
        if(&timer == &cpu.timer3 && tcnt == ocrNa)
        {
            if(com3a == 1) toggle_portc6(cpu);
            if(com3a == 2) clear_portc6(cpu);
            if(com3a == 3) set_portc6(cpu);
        }
        if(tcnt == ocrNa)
            tifr |= reg::bit::TIFR1::OCF1A;
        if(tcnt == ocrNb)
            tifr |= reg::bit::TIFR1::OCF1B;
        if(tcnt == ocrNc)
            tifr |= reg::bit::TIFR1::OCF1C;
    }

    timer.tcnt = tcnt;
    timer.count_down = count_down;
    if(tifr)
        cpu.schedule_interrupt_check();
    cpu.data[timer.tifrN_addr] |= tifr;
    cpu.data[timer.base_addr + 0x4] = uint8_t(tcnt >> 0);
    cpu.data[timer.base_addr + 0x5] = uint8_t(tcnt >> 8);
}

static void update_timer16(
    atmega32u4_t& cpu,
    atmega32u4_t::timer16_t& timer)
{
    uint32_t old_divider = timer.divider;
    // first compute what happened to tcnt/tifr during the cycles
    if(!(timer.divider == 0 || (cpu.data[timer.prr_addr] & timer.prr_mask)) &&
        cpu.cycle_count > timer.prev_update_cycle)
    {
        // timer clock is running and timer is not powered down...
        uint64_t cycles = cpu.cycle_count - timer.prev_update_cycle;
        update_timer16_state(cpu, timer, cycles);
    }
    timer.prev_update_cycle = cpu.cycle_count;

    // now set up timer state for next update

    uint32_t addr = timer.base_addr;
    uint32_t cs = cpu.data[addr + 0x1] & 0x7;
    timer.divider = get_divider(cs);

    if(timer.divider == 0 || (cpu.data[timer.prr_addr] & timer.prr_mask))
    {
        timer.prescaler_cycle = 0;
        timer.next_update_cycle = UINT64_MAX;
        return;
    }

    uint32_t icrN = word(cpu, addr + 0x6);
    uint32_t tccrNa = cpu.data[addr + 0x0];
    uint32_t tccrNb = cpu.data[addr + 0x1];
    uint32_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);

    uint32_t wgm_mask = 1 << wgm;
    if(old_divider == 0 || (wgm_mask & 0x1011)) // update ocrN immediately
        timer16_update_ocrN(cpu, timer, addr);
    timer.update_ocrN_at_bottom = ((wgm_mask & 0x0300) != 0);
    timer.update_ocrN_at_top = ((wgm_mask & 0xccee) != 0);
    timer.fast_pwm = (wgm_mask & 0xC0E0) != 0;
    timer.top_source_icr = (wgm_mask & 0x4500) != 0;

    if(timer.update_ocrN_at_bottom && timer.tcnt == 0)
        timer16_update_ocrN(cpu, timer, addr);

    process_wgm16(wgm, timer.top, timer.tov, timer.ocrNa, icrN);
    timer.phase_correct = (wgm_mask & 0x0f0e) != 0;
    if(!timer.phase_correct)
        timer.count_down = false;

    timer.com3a = tccrNa >> 6;

    // compute next update cycle

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
        // all ocrN's and tov are the same. set to period
        update_tcycles = timer.top;
        if(timer.phase_correct)
            update_tcycles *= 2;
    }

    if(update_tcycles == 0)
        timer.next_update_cycle = UINT64_MAX;
    else
    {
        uint64_t update_cycles = (uint64_t)update_tcycles * timer.divider - timer.prescaler_cycle;
        timer.next_update_cycle = cpu.cycle_count + update_cycles;
    }

    //timer.next_update_cycle = cpu.cycle_count + 1;
    cpu.peripheral_queue.schedule(
        timer.next_update_cycle,
        &timer == &cpu.timer1 ? PQ_TIMER1 : PQ_TIMER3);
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

        cpu.data[reg::addr::OCR4A] = (uint8_t)timer.ocrNa;
        cpu.data[reg::addr::OCR4B] = (uint8_t)timer.ocrNb;
        cpu.data[reg::addr::OCR4C] = (uint8_t)timer.ocrNc;
        cpu.data[reg::addr::OCR4D] = (uint8_t)timer.ocrNd;
    }
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

ARDENS_FORCEINLINE static void update_timer10_state(
    atmega32u4_t& cpu,
    atmega32u4_t::timer10_t& timer,
    uint64_t cycles)
{
    // find out how many timer cycles happened after prescaler

    // modify timer cycles by async clock
    if(cpu.pll_num12 > 0)
    {
        uint64_t t = cycles * cpu.pll_num12 + timer.async_cycle;
        uint64_t d = t / 12;
        uint64_t m = t % 12;
        timer.async_cycle = (uint32_t)m;
        cycles = d;
    }

    uint32_t timer_cycles = increase_counter(
        timer.divider_cycle, (uint32_t)cycles, timer.divider);

    auto tcnt = timer.tcnt;
    bool count_down = timer.count_down;
    bool phase_correct = timer.phase_correct;
    auto ocrNa = timer.ocrNa;
    auto ocrNb = timer.ocrNb;
    auto ocrNd = timer.ocrNd;
    auto com4a = timer.com4a;
    if(timer.enhc)
    {
        ocrNa /= 2;
        ocrNb /= 2;
        ocrNd /= 2;
    }
    auto tov = timer.tov;
    auto top = timer.top;
    uint8_t tifr = 0;
    bool compare_allowed = true;
    if(timer.compare_block_next_tick && timer_cycles > 0)
    {
        compare_allowed = false;
        timer.compare_block_next_tick = false;
    }

    while(timer_cycles > 0)
    {
        if(compare_allowed)
        {
            if(tcnt == ocrNa) tifr |= reg::bit::TIFR4::OCF4A;
            if(tcnt == ocrNb) tifr |= reg::bit::TIFR4::OCF4B;
            if(tcnt == ocrNd) tifr |= reg::bit::TIFR4::OCF4D;
        }
        if(count_down)
        {
            uint32_t stop = 0;
            if(ocrNa < tcnt) stop = std::max(stop, ocrNa);
            if(ocrNb < tcnt) stop = std::max(stop, ocrNb);
            if(ocrNd < tcnt) stop = std::max(stop, ocrNd);
            uint32_t t = tcnt - stop;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt -= t;
            if(tcnt == 0) tifr |= reg::bit::TIFR4::TOV4, count_down = false;
        }
        else if(tcnt == top)
        {
            if(phase_correct)
                count_down = true, --tcnt;
            else
                tifr |= 0x4, tcnt = 0;
            timer_cycles -= 1;
            if(timer.update_ocrN_at_top)
            {
                timer10_update_ocrN(cpu, timer);
                top = timer.top;
                tov = timer.tov;
                ocrNa = timer.ocrNa;
                ocrNb = timer.ocrNb;
                ocrNd = timer.ocrNd;
            }
            if(com4a == 1) set_portc(cpu, true, false);
            if(com4a == 2) set_portc7(cpu, true);
            if(com4a == 3) set_portc7(cpu, false);
        }
        else if(tcnt > top)
        {
            uint32_t t = 256;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt += t;
            tcnt &= 0x07ff;
            if(tcnt < 0x800) tifr |= reg::bit::TIFR4::TOV4;
        }
        else
        {
            uint32_t stop = top;
            if(ocrNa > tcnt) stop = std::min(stop, ocrNa);
            if(ocrNb > tcnt) stop = std::min(stop, ocrNb);
            if(ocrNd > tcnt) stop = std::min(stop, ocrNd);
            uint32_t t = stop - tcnt;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt += t;
        }
        if(compare_allowed && tcnt == ocrNa)
        {
            if(com4a == 1) set_portc(cpu, false, true);
            if(com4a == 2) set_portc7(cpu, false);
            if(com4a == 3) set_portc7(cpu, true);
        }
        compare_allowed = true;
    }

    timer.tcnt = tcnt;
    timer.count_down = count_down;
    if(tifr)
        cpu.schedule_interrupt_check();
    cpu.data[reg::addr::TIFR4] |= tifr;
    cpu.data[reg::addr::TCNT4] = uint8_t(tcnt >> 0);
}

void atmega32u4_t::update_timer4()
{
    uint32_t old_divider = timer4.divider;

    if(timer4.start_delay_cycles > 0)
    {
        uint64_t cycles = cycle_count - timer4.prev_update_cycle;
        if(cycles < timer4.start_delay_cycles)
        {
            timer4.start_delay_cycles -= uint32_t(cycles);
            timer4.prev_update_cycle = cycle_count;
            timer4.next_update_cycle = cycle_count + timer4.start_delay_cycles;
            peripheral_queue.schedule(timer4.next_update_cycle, PQ_TIMER4);
            return;
        }

        cycles -= timer4.start_delay_cycles;
        timer4.start_delay_cycles = 0;
        timer4.prev_update_cycle = cycle_count - cycles;
    }

    // first compute what happened to tcnt/tifr during the cycles
    if(!(timer4.divider == 0 || (data[reg::addr::PRR1] & reg::bit::PRR1::PRTIM4)))
    {
        // timer clock is running and timer is not powered down...
        uint64_t cycles = cycle_count - timer4.prev_update_cycle;
        update_timer10_state(*this, timer4, cycles);
    }
    timer4.prev_update_cycle = cycle_count;

    // now set up timer state for next update

    uint32_t tccr4a = data[reg::addr::TCCR4A];
    uint32_t tccr4b = data[reg::addr::TCCR4B];
    uint32_t tccr4c = data[reg::addr::TCCR4C];
    uint32_t tccr4d = data[reg::addr::TCCR4D];
    uint32_t tccr4e = data[reg::addr::TCCR4E];

    timer4.tlock = ((tccr4e & reg::bit::TCCR4E::TLOCK4) != 0);
    timer4.enhc = ((tccr4e & reg::bit::TCCR4E::ENHC4) != 0);

    uint32_t cs = tccr4b & 0xf;
    timer4.divider = (cs == 0 ? 0 : 1 << (cs - 1));
    bool timer_stopped = (timer4.divider == 0 || (data[reg::addr::PRR1] & reg::bit::PRR1::PRTIM4));

    if(timer_stopped)
    {
        timer4.divider_cycle = 0;
        timer4.async_cycle = 0;
    }

    bool pwm4x = ((tccr4b & reg::bit::TCCR4B::PWM4X) != 0);

    // for some reason, on actual Arduboys, pwm4x behaves as though it
    // were always set, not according to the datasheet. WHY??
    pwm4x = true;

    uint32_t wgm = tccr4d & 0x3;

    uint32_t wgm_mask = 1 << wgm;
    if(!pwm4x) // update ocrN immediately
        timer10_update_ocrN(*this, timer4);
    timer4.update_ocrN_at_top = false;
    timer4.update_ocrN_at_bottom = false;
    if(pwm4x)
    {
        if(wgm & 0x1)
            timer4.update_ocrN_at_bottom = true;
        else
            timer4.update_ocrN_at_top = true;
    }

    if(timer4.update_ocrN_at_bottom && timer4.tcnt == 0)
        timer10_update_ocrN(*this, timer4);

    timer4.top = timer4.ocrNc;
    if(pwm4x)
        timer4.top = std::max<uint32_t>(3, timer4.top);
    timer4.tov = timer4.top;

    timer4.phase_correct = false;
    if(!timer4.phase_correct)
        timer4.count_down = false;

    if(old_divider == 0 && timer4.divider == 1 && !timer_stopped)
    {
        timer4.start_delay_cycles = 3;
        timer4.next_update_cycle = cycle_count + timer4.start_delay_cycles;
        peripheral_queue.schedule(timer4.next_update_cycle, PQ_TIMER4);
        return;
    }

    if(timer_stopped)
    {
        // Keep buffered OCR values synchronized while stopped so the next
        // timer start sees the latest top and compare values.
        timer10_update_ocrN(*this, timer4);
        timer4.divider_cycle = 0;
        timer4.async_cycle = 0;
        timer4.next_update_cycle = UINT64_MAX;
        return;
    }

    // determine whether we are pwm-ing to sound pins
    // (pins connected and freq at least 20 kHz)
    update_sound();
    sound_pwm = false;
    uint32_t com = timer4.com4a = tccr4a >> 6;
    if((tccr4a & (reg::bit::TCCR4A::COM4A1 | reg::bit::TCCR4A::COM4A0)) != 0 && (tccr4a & reg::bit::TCCR4A::PWM4A) != 0)
    {
        uint32_t period = (timer4.top + 1) * timer4.divider;
        if(!pwm4x || wgm != 0) period *= 2; // not fast PWM
        if(timer4.enhc) period /= 2;

        if(!pwm4x)
        {
            sound_pwm_val = (com == 1 ? SOUND_GAIN / 2 : 0);
        }
        else
        {
            // use ocrNa_next to simulate fast update
            int32_t val = timer4.ocrNa_next * SOUND_GAIN / timer4.top;
            if(timer4.enhc) val /= 2;
            if(com == 1) val = val * 2 - SOUND_GAIN;
            if(com == 3) val = SOUND_GAIN - val;
            sound_pwm_val = (int16_t)val;
        }

        uint32_t max_period = 800; // 20kHz
        if(pll_num12 > 0)
            period *= 12, max_period *= pll_num12;
        if(period <= max_period)
            sound_pwm = true;
    }

    // compute next update cycle

    uint32_t update_tcycles = UINT32_MAX;
    if(timer4.count_down)
    {
        update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.tcnt - timer4.ocrNa);
        update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.tcnt - timer4.ocrNb);
        update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.tcnt - timer4.ocrNd);
    }
    else
    {
        if(timer4.phase_correct)
            update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.top - timer4.tcnt);
        else
            update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.tov - timer4.tcnt + 1);
        update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.ocrNa - timer4.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.ocrNb - timer4.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.ocrNd - timer4.tcnt);
        if(timer4.tcnt == timer4.top && timer4.top != 0)
            update_tcycles = 1;
    }
    if(update_tcycles == UINT32_MAX)
    {
        // all ocrN's and tov are the same. set to period
        update_tcycles = timer4.top;
        if(timer4.phase_correct)
            update_tcycles *= 2;
    }

    if(update_tcycles == 0)
        timer4.next_update_cycle = UINT64_MAX;
    else
    {
        uint64_t update_cycles = (uint64_t)update_tcycles * timer4.divider - timer4.divider_cycle;
        if(pll_num12 > 0)
        {
            update_cycles = update_cycles * 12 / pll_num12;
            update_cycles = std::max<uint64_t>(update_cycles, 1);
        }
        timer4.next_update_cycle = cycle_count + update_cycles;
    }
    peripheral_queue.schedule(timer4.next_update_cycle, PQ_TIMER4);
}

void atmega32u4_t::timer0_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    // give an extra cycle to timer
    // (reg write should hit at the end of the cycle)
    cpu.timer0.prev_update_cycle -= 1;
    cpu.update_timer0();

    cpu.data[ptr] = x;
    cpu.update_timer0();

    // take cycle back
    cpu.timer0.prev_update_cycle += 1;
}

void atmega32u4_t::timer0_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::TIFR0);
    x = (cpu.data[reg::addr::TIFR0] & ~x);
    cpu.data[reg::addr::TIFR0] = x;
}

uint8_t atmega32u4_t::timer0_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr)
{
    cpu.update_timer0();
    return cpu.data[ptr];
}

void atmega32u4_t::timer0_handle_st_tcnt(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    // Match the end-of-cycle write timing used by the wider timers.
    cpu.timer0.prev_update_cycle -= 1;
    cpu.update_timer0();
    cpu.data[ptr] = x;
    cpu.timer0.tcnt = x;
    cpu.update_timer0();
    cpu.timer0.prev_update_cycle += 1;
}

ARDENS_FORCEINLINE void atmega32u4_t::update_timer1()
{
    update_timer16(*this, timer1);
}

ARDENS_FORCEINLINE void atmega32u4_t::update_timer3()
{
    update_timer16(*this, timer3);
}

static void timer16_handle_st_regs(
    atmega32u4_t& cpu, atmega32u4_t::timer16_t& timer, uint16_t ptr, uint8_t x)
{
    uint32_t old_divider = timer.divider;
    // give an extra cycle to timer
    // (reg write should hit at the end of the cycle)
    timer.prev_update_cycle -= 1;
    update_timer16(cpu, timer);

    if( ptr >= timer.base_addr + 0x4 &&
        ptr <= timer.base_addr + 0xd &&
        (ptr & 1) == 0)
    {
        // write to 16-bit reg low byte
        cpu.data[ptr] = x;
        cpu.data[ptr + 1] = timer.temp;
        uint16_t val = ((uint16_t)timer.temp << 8) + x;
        if(ptr == timer.base_addr + 0x4)
            timer.tcnt = val;
        // OCR regs might be double-buffered
#if 0
        if(ptr == timer.base_addr + 0x8)
            timer.ocrNa = val;
        if(ptr == timer.base_addr + 0xa)
            timer.ocrNb = val;
        if(ptr == timer.base_addr + 0xc)
            timer.ocrNc = val;
#endif
        update_timer16(cpu, timer);
        // take cycle back
        timer.prev_update_cycle += 1;
        return;
    }
    // take cycle back
    timer.prev_update_cycle += 1;
    if( ptr >= timer.base_addr + 0x4 &&
        ptr <= timer.base_addr + 0xd &&
        (ptr & 1) == 1)
    {
        // write to 16-bit reg high byte
        timer.temp = x;
        return;
    }
    cpu.data[ptr] = x;
    update_timer16(cpu, timer);
    timer.prev_update_cycle += (old_divider == 0 && timer.divider != 0 ? 1 : 0);
}

void atmega32u4_t::timer1_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    timer16_handle_st_regs(cpu, cpu.timer1, ptr, x);
}

void atmega32u4_t::timer1_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::TIFR1);
    x = (cpu.data[reg::addr::TIFR1] & ~x);
    cpu.data[reg::addr::TIFR1] = x;
}

void atmega32u4_t::timer3_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    timer16_handle_st_regs(cpu, cpu.timer3, ptr, x);
}

void atmega32u4_t::timer3_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::TIFR3);
    x = (cpu.data[reg::addr::TIFR3] & ~x);
    cpu.data[reg::addr::TIFR3] = x;
}

static uint8_t timer16_handle_ld_reg16(
    atmega32u4_t& cpu, atmega32u4_t::timer16_t& timer, uint16_t ptr)
{
    update_timer16(cpu, timer);
    if(ptr >= timer.base_addr + 0x4 &&
        ptr <= timer.base_addr + 0xd &&
        (ptr & 1) == 0)
    {
        // read from 16-bit reg low byte
        timer.temp = cpu.data[ptr + 1];
        return cpu.data[ptr];
    }
    if(ptr >= timer.base_addr + 0x4 &&
        ptr <= timer.base_addr + 0xd &&
        (ptr & 1) == 1)
    {
        // read from 16-bit reg high byte
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

void atmega32u4_t::timer4_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    // give an extra cycle to timer
    // (reg write should hit at the end of the cycle)
    cpu.timer4.prev_update_cycle -= 1;
    cpu.update_timer4();

    if(ptr == reg::addr::TIFR4)
        x = (cpu.data[reg::addr::TIFR4] & ~x);
    cpu.data[ptr] = x;
    if(ptr == reg::addr::TCNT4)
    {
        uint32_t hi = cpu.data[reg::addr::TC4H] & (cpu.timer4.enhc ? (reg::bit::TC4H::TC4H0 | reg::bit::TC4H::TC4H1 | reg::bit::TC4H::TC4H2) : (reg::bit::TC4H::TC4H0 | reg::bit::TC4H::TC4H1));
        uint32_t tcnt = ((hi << 8) | x) & 0x7ff;
        cpu.timer4.compare_block_next_tick = true;
        if(cpu.timer4.divider == 0 || (cpu.data[reg::addr::PRR1] & reg::bit::PRR1::PRTIM4))
        {
            cpu.timer4.tcnt = tcnt;
            cpu.data[reg::addr::TCNT4] = uint8_t(tcnt >> 0);
            cpu.timer4.tcnt_write_pending = false;
            cpu.timer4.tcnt_write_pending_seen = false;
        }
        else
        {
            cpu.timer4.tcnt_write_pending = true;
            cpu.timer4.tcnt_write_pending_seen = false;
            cpu.timer4.tcnt_write_value = tcnt;
            cpu.data[reg::addr::TCNT4] = uint8_t(cpu.timer4.tcnt >> 0);
        }
    }
    cpu.update_timer4();

    // take cycle back
    cpu.timer4.prev_update_cycle += 1;
}

void atmega32u4_t::timer4_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::TIFR4);
    x = (cpu.data[reg::addr::TIFR4] & ~x);
    cpu.data[reg::addr::TIFR4] = x;
}

void atmega32u4_t::timer4_handle_st_ocrN(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    uint16_t ocr = ((uint16_t)cpu.data[reg::addr::TC4H] << 8);
    ocr &= (cpu.timer4.enhc ? 0x700 : 0x300);
    ocr |= x;
    if(ptr == reg::addr::OCR4A) cpu.timer4.ocrNa_next = ocr;
    if(ptr == reg::addr::OCR4B) cpu.timer4.ocrNb_next = ocr;
    if(ptr == reg::addr::OCR4C) cpu.timer4.ocrNc_next = ocr;
    if(ptr == reg::addr::OCR4D) cpu.timer4.ocrNd_next = ocr;
    cpu.update_timer4();
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
        cpu.data[reg::addr::TCNT4] = uint8_t(cpu.timer4.tcnt >> 0);
    }
    cpu.update_timer4();
    if(cpu.timer4.tcnt_write_pending)
        cpu.timer4.tcnt_write_pending_seen = true;
    return cpu.data[ptr];
}

}
