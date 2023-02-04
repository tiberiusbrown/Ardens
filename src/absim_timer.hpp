#include "absim.hpp"

namespace absim
{

static FORCEINLINE uint32_t increase_counter(uint32_t& counter, uint32_t inc, uint32_t top)
{
    uint32_t n = 0;
    uint32_t c = counter;
    c += inc;
    if(c >= top)
    {
        do
        {
            c -= top;
            ++n;
        } while(c >= top);
    }
    counter = c;
    return n;
}

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

static inline FORCEINLINE void timer16_update_ocrN(
    atmega32u4_t& cpu,
    atmega32u4_t::timer16_t& timer,
    uint32_t addr)
{
    timer.ocrNa = word(cpu, addr + 0x8);
    timer.ocrNb = word(cpu, addr + 0xa);
    timer.ocrNc = word(cpu, addr + 0xc);
}

static FORCEINLINE void cycle_timer16(
    atmega32u4_t& cpu,
    atmega32u4_t::timer16_t& timer,
    uint32_t addr,
    uint32_t tifrN_addr,
    uint32_t powered_down,
    uint32_t cycles)
{
    //if(cpu.just_written >= addr && cpu.just_written <= addr + 0x0d)
    if((cpu.just_written >> 4) == (addr >> 4))
    {
        uint32_t cs = cpu.data[addr + 0x1] & 0x7;
        timer.divider = get_divider(cs);
        cpu.update_sleep_min_cycles();
        if(cs == 0) return;

        uint32_t icrN = word(cpu, addr + 0x6);
        uint32_t tccrNa = cpu.data[addr + 0x0];
        uint32_t tccrNb = cpu.data[addr + 0x1];
        uint32_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);

        uint32_t wgm_mask = 1 << wgm;
        if(wgm_mask & 0x1011) // update ocrN immediately
            timer16_update_ocrN(cpu, timer, addr);
        timer.update_ocrN_at_bottom = ((wgm_mask & 0x0300) != 0);
        timer.update_ocrN_at_top    = ((wgm_mask & 0xccee) != 0);

        process_wgm16(wgm, timer.top, timer.tov, timer.ocrNa, icrN);
        timer.phase_correct = (1 << wgm) & 0x0f0e;
        if(!timer.phase_correct)
            timer.count_down = false;
    }

    if(powered_down) return;

    if(timer.divider == 0) return;

    uint32_t timer_cycles = increase_counter(
        timer.divider_cycle, cycles, timer.divider);
    if(timer_cycles == 0) return;

    uint8_t& tifrN = cpu.data[tifrN_addr];

    // writing a logic 1 to TOV1 clears it
    if(cpu.just_written == tifrN_addr && (tifrN & 0x1))
        tifrN &= ~0x1;

    uint32_t t = word(cpu, addr + 0x4);

    uint32_t ocrNa = word(cpu, addr + 0x8);
    uint32_t ocrNb = word(cpu, addr + 0xa);
    uint32_t ocrNc = word(cpu, addr + 0xc);

    for(uint32_t i = 0; i < timer_cycles; ++i)
    {
        if(t == timer.tov)
            tifrN |= (1 << 0);
        if(t == timer.ocrNa)
            tifrN |= (1 << 1);
        if(t == timer.ocrNb)
            tifrN |= (1 << 2);
        if(t == timer.ocrNc)
            tifrN |= (1 << 3);

        if(timer.count_down)
        {
            if(t == 0)
            {
                if(timer.update_ocrN_at_bottom)
                    timer16_update_ocrN(cpu, timer, addr);
                t = 1;
                timer.count_down = false;
            }
            else
                --t;
        }
        else
        {
            if(t == timer.top)
            {
                if(timer.update_ocrN_at_top)
                    timer16_update_ocrN(cpu, timer, addr);
                if(timer.phase_correct)
                    timer.count_down = true;
                else
                    t = 0;
            }
            else
                ++t;
        }
    }

    cpu.data[addr + 0x4 + 0] = uint8_t(t >> 0);
    cpu.data[addr + 0x4 + 1] = uint8_t(t >> 8);
}

FORCEINLINE void atmega32u4_t::cycle_timer1(uint32_t cycles)
{
    uint32_t prr0 = data[0x64];
    constexpr uint32_t prtim1 = 1 << 3;

    cycle_timer16(*this, timer1, 0x80, 0x36, prr0 & prtim1, cycles);
}

FORCEINLINE void atmega32u4_t::cycle_timer3(uint32_t cycles)
{
    uint32_t prr1 = data[0x65];
    constexpr uint32_t prtim3 = 1 << 3;

    cycle_timer16(*this, timer3, 0x90, 0x38, prr1 & prtim3, cycles);
}

}
