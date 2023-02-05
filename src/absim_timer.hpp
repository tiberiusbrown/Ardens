#include "absim.hpp"

namespace absim
{

static FORCEINLINE uint32_t word(atmega32u4_t const& cpu, uint32_t addr)
{
    uint32_t lo = cpu.data[addr + 0];
    uint32_t hi = cpu.data[addr + 1];
    return lo | (hi << 8);
}

static FORCEINLINE uint32_t get_divider(uint32_t cs)
{
    assert((cs & ~7) == 0);
    static constexpr uint32_t DIVIDERS[8] =
    {
        0, 1, 8, 64, 256, 1024, 0, 0,
    };
    return DIVIDERS[cs];
}

FORCEINLINE void atmega32u4_t::cycle_timer0(uint32_t cycles)
{
    if(just_written == 0x45)
    {
        uint32_t cs = data[0x45] & 0x7;
        timer0_divider = get_divider(cs);
        update_sleep_min_cycles();
        if(cs == 0) return;
    }
    if(timer0_divider == 0) return;

    // writing a logic 1 to TOV0 clears it
    if(just_written == 0x35 && (tifr0() & 0x1))
        tifr0() &= ~0x1;

    // timer0 is powered down
    uint32_t prr0 = data[0x64];
    constexpr uint32_t prtim0 = 1 << 5;
    if(prr0 & prtim0)
        return;

    uint32_t timer_cycles = increase_counter(
        timer0_divider_cycle, cycles, timer0_divider);
    if(timer_cycles == 0) return;

    uint32_t wgm = (tccr0a() & 0x3) | ((tccr0b() >> 1) & 0x4);

    uint32_t top = 0xff;
    uint32_t tov = 0xff;

    // skip compare output modes since they are not connected to Arduboy pins

    switch(wgm)
    {
    case 0x0: // normal
        break;
    case 0x1: // PWM, phase correct
        tov = 0x00;
        break;
    case 0x2: // CTC
        top = ocr0a();
        break;
    case 0x3: // fast PWM
        break;
    case 0x5: // PWM, phase correct
        tov = 0x00;
        top = ocr0a();
        break;
    case 0x7: // fast PWM
        top = ocr0a();
        break;
    default:
        break;
    }

    if((wgm & 0x3) != 1)
        timer0_count_down = false;

    uint8_t& t = tcnt0();

    for(uint32_t i = 0; i < timer_cycles; ++i)
    {
        if(t == tov)
            tifr0() |= (1 << 0);
        if(t == ocr0a())
            tifr0() |= (1 << 1);
        if(t == ocr0b())
            tifr0() |= (1 << 2);

        if(t == top)
        {
            if((wgm & 0x3) == 1)
            {
                timer0_count_down = !timer0_count_down;
                t = timer0_count_down ? t - 1 : t + 1;
            }
            else
                t = 0;
        }
        else
        {
            t = timer0_count_down ? t - 1 : t + 1;
        }
    }
}

static FORCEINLINE void process_wgm16(
    uint32_t wgm, uint32_t& top, uint32_t& tov, uint32_t ocr, uint32_t icr)
{
    top = 0xffff;
    tov = 0xffff;

    switch(wgm)
    {
    case 0x0: // normal
        break;
    case 0x1: // PWM, Phase Correct, 8-bit
        top = 0x00ff;
        tov = 0x0000;
        break;
    case 0x2: // PWM, Phase Correct, 9-bit
        top = 0x01ff;
        tov = 0x0000;
        break;
    case 0x3: // PWM, Phase Correct, 10-bit
        top = 0x03ff;
        tov = 0x0000;
        break;
    case 0x4: // CTC
        top = ocr;
        break;
    case 0x5: // Fast PWM, 8-bit
        top = 0x00ff;
        break;
    case 0x6: // Fast PWM, 9-bit
        top = 0x01ff;
        break;
    case 0x7: // Fast PWM, 10-bit
        top = 0x03ff;
        break;
    case 0x8: // PWM, Phase and Frequency Correct
        top = icr;
        tov = 0x0000;
        break;
    case 0x9: // PWM, Phase and Frequency Correct
        top = ocr;
        tov = 0x0000;
        break;
    case 0xa: // PWM, Phase Correct
        top = icr;
        tov = 0x0000;
        break;
    case 0xb: // PWM, Phase Correct
        top = ocr;
        tov = 0x0000;
        break;
    case 0xc: // CTC
        top = icr;
        break;
    case 0xd: // (Reserved)
        break;
    case 0xe: // Fast PWM
        top = icr;
        break;
    case 0xf: // Fast PWM
        top = ocr;
        break;
    default:
        break;
    }
}

static FORCEINLINE void timer16_update_ocrN(
    atmega32u4_t& cpu,
    atmega32u4_t::timer16_t& timer,
    uint32_t addr)
{
    timer.ocrNa = std::max<uint32_t>(3, word(cpu, addr + 0x8));
    timer.ocrNb = std::max<uint32_t>(3, word(cpu, addr + 0xa));
    timer.ocrNc = std::max<uint32_t>(3, word(cpu, addr + 0xc));

    uint32_t icrN = word(cpu, addr + 0x6);
    uint32_t tccrNa = cpu.data[addr + 0x0];
    uint32_t tccrNb = cpu.data[addr + 0x1];
    uint32_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);

    process_wgm16(wgm, timer.top, timer.tov, timer.ocrNa, icrN);
}

static FORCEINLINE uint32_t timer16_period(
    atmega32u4_t const& cpu,
    atmega32u4_t::timer16_t const& timer)
{
    uint32_t period = UINT32_MAX;
    uint32_t timsk = cpu.data[timer.timskN_addr];
    if(timsk & 0x01) period = std::min(period, timer.tov);
    if(timsk & 0x02) period = std::min(period, timer.ocrNa);
    if(timsk & 0x04) period = std::min(period, timer.ocrNb);
    if(timsk & 0x08) period = std::min(period, timer.ocrNc);
    return period;
}

static FORCEINLINE void update_timer16_state(
    atmega32u4_t& cpu,
    atmega32u4_t::timer16_t& timer,
    uint64_t cycles)
{
    // find out how many timer cycles happened after prescaler
    uint32_t timer_cycles = increase_counter(timer.divider_cycle, cycles, timer.divider);

    auto tcnt = timer.tcnt;
    bool count_down = timer.count_down;
    bool phase_correct = timer.phase_correct;
    auto ocrNa = timer.ocrNa;
    auto ocrNb = timer.ocrNb;
    auto ocrNc = timer.ocrNc;
    auto tov = timer.tov;
    auto top = timer.top;
    uint8_t tifr = cpu.data[timer.tifrN_addr] & 0xf;

    auto minstop = std::min(
        std::min(ocrNa, tov),
        std::min(ocrNb, ocrNc));
    auto maxstop = std::max(
        std::max(ocrNa, tov),
        std::max(ocrNb, ocrNc));

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
            if(tcnt == 0) tifr |= 0x1, count_down = false;
        }
        else if(tcnt > top)
        {
            uint32_t t = 256;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt += t;
            tcnt &= 0xffff;
            if(tcnt < 0x10000) tifr |= 0x1;
        }
        else
        {
            uint32_t stop = top + 1;
            if(ocrNa > tcnt) stop = std::min(stop, ocrNa);
            if(ocrNb > tcnt) stop = std::min(stop, ocrNb);
            if(ocrNc > tcnt) stop = std::min(stop, ocrNc);
            uint32_t t = stop - tcnt;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt += t;
            if(tcnt == top + 1)
                tifr |= 0x1, tcnt = 0;
        }
        if(tcnt == ocrNa) tifr |= 0x2;
        if(tcnt == ocrNb) tifr |= 0x4;
        if(tcnt == ocrNc) tifr |= 0x8;
    }

    timer.tcnt = tcnt;
    timer.count_down = count_down;
    cpu.data[timer.tifrN_addr] |= tifr;
    cpu.data[timer.base_addr + 0x4] = uint8_t(tcnt >> 0);
    cpu.data[timer.base_addr + 0x5] = uint8_t(tcnt >> 8);
}

static FORCEINLINE uint32_t min_nonzero(uint32_t a, uint32_t top, int32_t b)
{
    if(b <= 0) return std::min(a, top);
    return std::min(a, (uint32_t)b);
}

static void update_timer16(
    atmega32u4_t& cpu,
    atmega32u4_t::timer16_t& timer)
{
    // first compute what happened to tcnt/tifr during the cycles
    if(!(timer.divider == 0 || (cpu.data[timer.prr_addr] & timer.prr_mask)))
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
        timer.next_update_cycle = UINT64_MAX;
        return;
    }

    uint32_t icrN = word(cpu, addr + 0x6);
    uint32_t tccrNa = cpu.data[addr + 0x0];
    uint32_t tccrNb = cpu.data[addr + 0x1];
    uint32_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);

    uint32_t wgm_mask = 1 << wgm;
    if(wgm_mask & 0x1011) // update ocrN immediately
        timer16_update_ocrN(cpu, timer, addr);
    timer.update_ocrN_at_bottom = ((wgm_mask & 0x0300) != 0);
    timer.update_ocrN_at_top = ((wgm_mask & 0xccee) != 0);

    if(timer.update_ocrN_at_bottom && timer.tcnt == 0)
        timer16_update_ocrN(cpu, timer, addr);
    if(timer.update_ocrN_at_top && timer.tcnt == timer.top)
        timer16_update_ocrN(cpu, timer, addr);

    process_wgm16(wgm, timer.top, timer.tov, timer.ocrNa, icrN);
    timer.phase_correct = (1 << wgm) & 0x0f0e;
    if(!timer.phase_correct)
        timer.count_down = false;

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
            update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tov - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNa - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNb - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNc - timer.tcnt);
    }
    if(update_tcycles == UINT32_MAX)
    {
        // all ocrN's and tov are the same. set to period
        update_tcycles = timer.top;
        if(timer.phase_correct)
            update_tcycles *= 2;
    }

    if(update_tcycles > timer.top) __debugbreak();

    uint64_t update_cycles = (uint64_t)update_tcycles * timer.divider - timer.divider_cycle;

    timer.next_update_cycle = cpu.cycle_count + update_cycles;
}

FORCEINLINE void atmega32u4_t::update_timer1()
{
    update_timer16(*this, timer1);
}

FORCEINLINE void atmega32u4_t::update_timer3()
{
    update_timer16(*this, timer3);
}

void atmega32u4_t::timer1_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr] = x;
    update_timer16(cpu, cpu.timer1);
}

void atmega32u4_t::timer3_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr] = x;
    update_timer16(cpu, cpu.timer3);
}

uint8_t atmega32u4_t::timer1_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr)
{
    update_timer16(cpu, cpu.timer1);
    return cpu.data[ptr];
}

uint8_t atmega32u4_t::timer3_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr)
{
    update_timer16(cpu, cpu.timer3);
    return cpu.data[ptr];
}

}
