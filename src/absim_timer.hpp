#include "absim.hpp"

namespace absim
{

static ARDENS_FORCEINLINE uint32_t word(atmega32u4_t const& cpu, uint32_t addr)
{
    uint32_t lo = cpu.data[addr + 0];
    uint32_t hi = cpu.data[addr + 1];
    return lo | (hi << 8);
}

static ARDENS_FORCEINLINE uint32_t get_divider(uint32_t cs)
{
    assert((cs & ~7) == 0);
    static constexpr uint32_t DIVIDERS[8] =
    {
        0, 1, 8, 64, 256, 1024, 0, 0,
    };
    return DIVIDERS[cs];
}

static ARDENS_FORCEINLINE uint32_t min_nonzero(uint32_t a, uint32_t top, int32_t b)
{
    if(b <= 0) return std::min(a, top);
    return std::min(a, (uint32_t)b);
}

static ARDENS_FORCEINLINE void process_wgm8(
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

    top = std::max<uint32_t>(3, top);
}

static ARDENS_FORCEINLINE void timer8_update_ocrN(
    atmega32u4_t& cpu,
    atmega32u4_t::timer8_t& timer)
{
    timer.ocrNa = cpu.data[0x47];
    timer.ocrNb = cpu.data[0x48];

    uint32_t tccr0a = cpu.data[0x44];
    uint32_t tccr0b = cpu.data[0x45];
    uint32_t wgm = (tccr0a & 0x3) | ((tccr0b >> 1) & 0x4);
    process_wgm8(wgm, timer.top, timer.tov, timer.ocrNa);
}

static ARDENS_FORCEINLINE void update_timer8_state(
    atmega32u4_t& cpu,
    atmega32u4_t::timer8_t& timer,
    uint64_t cycles)
{
    // find out how many timer cycles happened after prescaler
    uint32_t timer_cycles = increase_counter(timer.divider_cycle, cycles, timer.divider);

    auto tcnt = timer.tcnt;
    bool count_down = timer.count_down;
    bool phase_correct = timer.phase_correct;
    auto ocrNa = timer.ocrNa;
    auto ocrNb = timer.ocrNb;
    auto tov = timer.tov;
    auto top = timer.top;
    uint8_t tifr = cpu.data[0x35] & 0x7;

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
                tifr |= 0x1, count_down = false;
        }
        else if(tcnt > top)
        {
            uint32_t t = 16;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt += t;
            tcnt &= 0xff;
            if(tcnt < 0x100)
                tifr |= 0x1;
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
                    count_down = true;
                else
                    tifr |= 0x1, tcnt = 0;
            }
        }
        if(tcnt == ocrNa) tifr |= 0x2;
        if(tcnt == ocrNb) tifr |= 0x4;
    }

    timer.tcnt = tcnt;
    timer.count_down = count_down;
    cpu.data[0x35] |= tifr;
    cpu.data[0x46] = uint8_t(tcnt);
}

void atmega32u4_t::update_timer0()
{
    // first compute what happened to tcnt/tifr during the cycles
    if(!(timer0.divider == 0 || (data[0x64] & (1 << 5))))
    {
        // timer clock is running and timer is not powered down...
        uint64_t cycles = cycle_count - timer0.prev_update_cycle;
        update_timer8_state(*this, timer0, cycles);
    }
    timer0.prev_update_cycle = cycle_count;

    // now set up timer state for next update

    uint32_t cs = data[0x45] & 0x7;
    timer0.divider = get_divider(cs);

    if(timer0.divider == 0 || (data[0x64] & (1 << 5)))
    {
        timer0.next_update_cycle = UINT64_MAX;
        return;
    }

    uint32_t tccr0a = data[0x44];
    uint32_t tccr0b = data[0x45];
    uint32_t wgm = (tccr0a & 0x3) | ((tccr0b >> 1) & 0x4);

    uint32_t wgm_mask = 1 << wgm;
    if(wgm_mask & 0x5) // update ocrN immediately
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
            update_tcycles = min_nonzero(update_tcycles, timer0.top, timer0.tov - timer0.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer0.top, timer0.ocrNa - timer0.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer0.top, timer0.ocrNb - timer0.tcnt);
    }
    if(update_tcycles == UINT32_MAX)
    {
        // all ocrN's and tov are the same. set to period
        update_tcycles = timer0.top;
        if(timer0.phase_correct)
            update_tcycles *= 2;
    }

    uint64_t update_cycles = (uint64_t)update_tcycles * timer0.divider - timer0.divider_cycle;

    timer0.next_update_cycle = cycle_count + update_cycles;
}

static ARDENS_FORCEINLINE void process_wgm16(
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

    top = std::max<uint32_t>(3, ttop);
    tov = ttov;
}

static ARDENS_FORCEINLINE void timer16_update_ocrN(
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

static ARDENS_FORCEINLINE uint32_t timer16_period(
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

static ARDENS_FORCEINLINE void toggle_portc6(atmega32u4_t& cpu)
{
    if(!(cpu.data[0x27] & 0x40)) return;
    cpu.data[0x28] ^= (1 << 6);
}

static ARDENS_FORCEINLINE void clear_portc6(atmega32u4_t& cpu)
{
    if(!(cpu.data[0x27] & 0x40)) return;
    cpu.data[0x28] &= ~(1 << 6);
}

static ARDENS_FORCEINLINE void set_portc6(atmega32u4_t& cpu)
{
    if(!(cpu.data[0x27] & 0x40)) return;
    cpu.data[0x28] |= (1 << 6);
}

static ARDENS_FORCEINLINE void update_timer16_state(
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
    auto com3a = timer.com3a;
    uint8_t tifr = cpu.data[timer.tifrN_addr] & 0xf;

    while(timer_cycles > 0)
    {
        if(tcnt == ocrNa) tifr |= 0x2;
        if(tcnt == ocrNb) tifr |= 0x4;
        if(tcnt == ocrNc) tifr |= 0x8;
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
            if(&timer == &cpu.timer3 && tcnt == ocrNa)
            {
                if(com3a == 1) toggle_portc6(cpu);
                if(com3a == 2) clear_portc6(cpu);
                if(com3a == 3) set_portc6(cpu);
            }
        }
        else if(tcnt == top)
        {
            if(phase_correct)
                count_down = true, --tcnt;
            else
                tifr |= 0x1, tcnt = 0;
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
            if(tcnt < 0x10000) tifr |= 0x1;
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
            if(&timer == &cpu.timer3 && tcnt == ocrNa)
            {
                if(com3a == 1) toggle_portc6(cpu);
                if(com3a == 2) clear_portc6(cpu);
                if(com3a == 3) set_portc6(cpu);
            }
            if(&timer == &cpu.timer3 && !phase_correct && tcnt == top + 1)
            {
                if(com3a == 1) toggle_portc6(cpu);
                if(com3a == 2) set_portc6(cpu);
                if(com3a == 3) clear_portc6(cpu);
            }
        }
    }

    timer.tcnt = tcnt;
    timer.count_down = count_down;
    cpu.data[timer.tifrN_addr] |= tifr;
    cpu.data[timer.base_addr + 0x4] = uint8_t(tcnt >> 0);
    cpu.data[timer.base_addr + 0x5] = uint8_t(tcnt >> 8);
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
            update_tcycles = min_nonzero(update_tcycles, timer.top, timer.tov - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNa - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNb - timer.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer.top, timer.ocrNc - timer.tcnt);
        if(timer.tcnt == timer.top)
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
        uint64_t update_cycles = (uint64_t)update_tcycles * timer.divider - timer.divider_cycle;
        timer.next_update_cycle = cpu.cycle_count + update_cycles;
    }
}

static ARDENS_FORCEINLINE void timer10_update_ocrN(
    atmega32u4_t& cpu,
    atmega32u4_t::timer10_t& timer)
{
    if(!timer.tlock)
    {
        timer.ocrNa = timer.ocrNa_next;
        timer.ocrNb = timer.ocrNb_next;
        timer.ocrNc = timer.ocrNc_next;
        timer.ocrNd = timer.ocrNd_next;

        cpu.data[0xcf] = (uint8_t)timer.ocrNa;
        cpu.data[0xd0] = (uint8_t)timer.ocrNb;
        cpu.data[0xd1] = (uint8_t)timer.ocrNc;
        cpu.data[0xd2] = (uint8_t)timer.ocrNd;
    }
}

static ARDENS_FORCEINLINE void set_portc(atmega32u4_t& cpu, bool c6, bool c7)
{
    uint8_t c = 0;
    if(c6) c |= 0x40;
    if(c7) c |= 0x80;
    uint8_t m = cpu.data[0x27];
    c &= m;
    uint8_t t = cpu.data[0x28];
    t &= ~m;
    t |= c;
    cpu.data[0x28] = t;
}

static ARDENS_FORCEINLINE void set_portc(atmega32u4_t& cpu, bool c6)
{
    uint8_t c = 0;
    if(c6) c |= 0x40;
    if(!(cpu.data[0x27] & 0x40)) return;
    c &= 0x40;
    uint8_t t = cpu.data[0x28];
    t &= ~0x40;
    t |= c;
    cpu.data[0x28] = t;
}

static ARDENS_FORCEINLINE void update_timer10_state(
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

    uint32_t timer_cycles = increase_counter(timer.divider_cycle, cycles, timer.divider);

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
    uint8_t tifr = cpu.data[0x39] & 0xe4;
    uint8_t& portc = cpu.data[0x28];
    uint8_t portc_mask = cpu.data[0x27];

    while(timer_cycles > 0)
    {
        if(tcnt == ocrNa) tifr |= 0x40;
        if(tcnt == ocrNb) tifr |= 0x20;
        if(tcnt == ocrNd) tifr |= 0x80;
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
            if(tcnt == 0) tifr |= 0x4, count_down = false;
            if(tcnt == ocrNa)
            {
                if(com4a == 1) set_portc(cpu, true, false);
                if(com4a == 2) set_portc(cpu, true);
                if(com4a == 3) set_portc(cpu, false);
            }
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
        }
        else if(tcnt > top)
        {
            uint32_t t = 256;
            t = std::min(t, timer_cycles);
            timer_cycles -= t;
            tcnt += t;
            tcnt &= 0x07ff;
            if(tcnt < 0x800) tifr |= 0x4;
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
            if(tcnt == ocrNa)
            {
                if(com4a == 1) set_portc(cpu, false, true);
                if(com4a == 2) set_portc(cpu, false);
                if(com4a == 3) set_portc(cpu, true);
            }
            if(!phase_correct && tcnt == top + 1)
            {
                if(com4a == 1) set_portc(cpu, true, false);
                if(com4a == 2) set_portc(cpu, true);
                if(com4a == 3) set_portc(cpu, false);
            }
        }
    }

    timer.tcnt = tcnt;
    timer.count_down = count_down;
    cpu.data[0x39] |= tifr;
    cpu.data[0xbe] = uint8_t(tcnt >> 0);
}

void atmega32u4_t::update_timer4()
{
    // first compute what happened to tcnt/tifr during the cycles
    if(!(timer4.divider == 0 || (data[0x65] & 0x10)))
    {
        // timer clock is running and timer is not powered down...
        uint64_t cycles = cycle_count - timer4.prev_update_cycle;
        update_timer10_state(*this, timer4, cycles);
    }
    timer4.prev_update_cycle = cycle_count;

    // now set up timer state for next update

    uint32_t tccr4a = data[0xc0];
    uint32_t tccr4b = data[0xc1];
    uint32_t tccr4c = data[0xc2];
    uint32_t tccr4d = data[0xc3];
    uint32_t tccr4e = data[0xc4];

    timer4.tlock = ((tccr4e & 0x80) != 0);
    timer4.enhc = ((tccr4e & 0x40) != 0);

    uint32_t cs = tccr4b & 0xf;
    timer4.divider = (cs == 0 ? 0 : 1 << (cs - 1));

    if(timer4.divider == 0 || (data[0x65] & 0x10))
    {
        timer4.next_update_cycle = UINT64_MAX;
        return;
    }

    bool pwm4x = ((tccr4b & 0x80) != 0);

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

    timer4.top = std::max<uint32_t>(3, timer4.ocrNc);
    timer4.tov = timer4.top;
    if(pwm4x && (wgm & 0x1))
        timer4.tov = 0;

    timer4.phase_correct = (pwm4x && wgm == 1);
    if(!timer4.phase_correct)
        timer4.count_down = false;

    // determine whether we are pwm-ing to sound pins
    // (pins connected and freq at least 20 kHz)
    sound_pwm = false;
    uint32_t com = timer4.com4a = tccr4a >> 6;
    if((tccr4a & 0xc0) != 0 && (tccr4a & 0x2) != 0)
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

    // shortcut: if no interrupts are enabled, just update occasionally
    if(timsk4() == 0)
    {
        timer4.next_update_cycle = cycle_count + 65536;
        return;
    }

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
            update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.tov - timer4.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.ocrNa - timer4.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.ocrNb - timer4.tcnt);
        update_tcycles = min_nonzero(update_tcycles, timer4.top, timer4.ocrNd - timer4.tcnt);
        if(timer4.tcnt == timer4.top)
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
            update_cycles = std::max<uint32_t>(update_cycles, 1);
        }
        timer4.next_update_cycle = cycle_count + update_cycles;
    }
}

void atmega32u4_t::timer0_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr] = x;
    cpu.update_timer0();
}

void atmega32u4_t::timer0_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == 0x35);
    x = (cpu.data[0x35] & ~x);
    cpu.data[0x35] = x;
}

uint8_t atmega32u4_t::timer0_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr)
{
    cpu.update_timer0();
    return cpu.data[ptr];
}

ARDENS_FORCEINLINE void atmega32u4_t::update_timer1()
{
    update_timer16(*this, timer1);
}

ARDENS_FORCEINLINE void atmega32u4_t::update_timer3()
{
    update_timer16(*this, timer3);
}

void atmega32u4_t::timer1_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr] = x;
    update_timer16(cpu, cpu.timer1);
}

void atmega32u4_t::timer1_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == 0x36);
    x = (cpu.data[0x36] & ~x);
    cpu.data[0x36] = x;
}

void atmega32u4_t::timer3_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    cpu.data[ptr] = x;
    update_timer16(cpu, cpu.timer3);
}

void atmega32u4_t::timer3_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == 0x38);
    x = (cpu.data[0x38] & ~x);
    cpu.data[0x38] = x;
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

void atmega32u4_t::timer4_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    if(ptr == 0x39)
        x = (cpu.data[0x39] & ~x);
    cpu.data[ptr] = x;
    cpu.update_timer4();
}

void atmega32u4_t::timer4_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == 0x39);
    x = (cpu.data[0x39] & ~x);
    cpu.data[0x39] = x;
}

void atmega32u4_t::timer4_handle_st_ocrN(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    uint16_t ocr = ((uint16_t)cpu.data[0xbf] << 8);
    ocr &= (cpu.timer4.enhc ? 0x700 : 0x300);
    ocr |= x;
    if(ptr == 0xcf) cpu.timer4.ocrNa_next = ocr;
    if(ptr == 0xd0) cpu.timer4.ocrNb_next = ocr;
    if(ptr == 0xd1) cpu.timer4.ocrNc_next = ocr;
    if(ptr == 0xd2) cpu.timer4.ocrNd_next = ocr;
    cpu.update_timer4();
}

uint8_t atmega32u4_t::timer4_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr)
{
    cpu.update_timer4();
    return cpu.data[ptr];
}

}
