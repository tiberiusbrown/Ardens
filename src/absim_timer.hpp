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

    uint32_t wgm = (tccr0a() & 0x3) | ((tccr0b() >> 1) & 0x4);

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

// TODO: cache this stuff
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

static inline FORCEINLINE void cycle_timer1_or_timer3(atmega32u4_t& cpu, uint32_t cycles,
    uint32_t   tccrNa_addr,
    uint32_t   tccrNb_addr,
    uint32_t   tccrNc_addr,
    uint32_t   tifrN_addr,
    uint32_t   icrN_addr,
    uint32_t   ocrNa_addr,
    uint32_t   ocrNb_addr,
    uint32_t   ocrNc_addr,
    uint32_t   tcntN_addr,
    bool&      count_down,
    bool       phase_correct,
    uint32_t   top,
    uint32_t   tov
)
{
    uint8_t& tifrN = cpu.data[tifrN_addr];

    // writing a logic 1 to TOV1 clears it
    if(cpu.just_written == tifrN_addr && (tifrN & 0x1))
        tifrN &= ~0x1;

    uint32_t ocrNa = word(cpu, ocrNa_addr);
    uint32_t ocrNb = word(cpu, ocrNb_addr);
    uint32_t ocrNc = word(cpu, ocrNc_addr);

    uint32_t t = word(cpu, tcntN_addr);

    for(uint32_t i = 0; i < cycles; ++i)
    {
        if(t == tov)
            tifrN |= (1 << 0);
        if(t == ocrNa)
            tifrN |= (1 << 1);
        if(t == ocrNb)
            tifrN |= (1 << 2);
        if(t == ocrNc)
            tifrN |= (1 << 3);

        if(t == top)
        {
            if(phase_correct)
            {
                count_down = !count_down;
                t = count_down ? t - 1 : t + 1;
            }
            else
                t = 0;
        }
        else
        {
            t = count_down ? t - 1 : t + 1;
        }
    }

    cpu.data[tcntN_addr + 0] = uint8_t(t >> 0);
    cpu.data[tcntN_addr + 1] = uint8_t(t >> 8);
}

void FORCEINLINE atmega32u4_t::cycle_timer1(uint32_t cycles)
{
    if(just_written >= 0x80 && just_written <= 0x8d)
    {
        uint32_t cs = data[0x81] & 0x7;
        timer1_divider = get_divider(cs);
        update_sleep_min_cycles();
        if(cs == 0) return;

        uint32_t icrN = word(*this, 0x86);
        uint32_t ocrNa = word(*this, 0x88);
        uint32_t tccrNa = data[0x80];
        uint32_t tccrNb = data[0x81];
        uint32_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);
        process_wgm16(wgm, timer1_top, timer1_tov, ocrNa, icrN);
        timer1_phase_correct = ((wgm & 0x3) == 1);
        if(!timer1_phase_correct)
            timer1_count_down = false;
    }
    if(timer1_divider == 0) return;

    uint32_t timer_cycles = increase_counter(
        timer1_divider_cycle, cycles, timer1_divider);
    if(timer_cycles == 0) return;

    uint32_t prr0 = data[0x64];
    constexpr uint32_t prtim1 = 1 << 3;
    if(prr0 & prtim1) return;

    cycle_timer1_or_timer3(*this, timer_cycles,
        0x80, 0x81, 0x82,
        0x36, 0x86,
        0x88, 0x8a, 0x8c,
        0x84,
        timer1_count_down,
        timer1_phase_correct,
        timer1_top,
        timer1_tov);
}

void FORCEINLINE atmega32u4_t::cycle_timer3(uint32_t cycles)
{
    if(just_written >= 0x90 && just_written <= 0x9d)
    {
        uint32_t cs = data[0x91] & 0x7;
        timer3_divider = get_divider(cs);
        update_sleep_min_cycles();
        if(cs == 0) return;

        uint32_t icrN = word(*this, 0x96);
        uint32_t ocrNa = word(*this, 0x98);
        uint32_t tccrNa = data[0x90];
        uint32_t tccrNb = data[0x91];
        uint32_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);
        process_wgm16(wgm, timer3_top, timer3_tov, ocrNa, icrN);
        timer3_phase_correct = ((wgm & 0x3) == 1);
        if(!timer3_phase_correct)
            timer3_count_down = false;
    }
    if(timer3_divider == 0) return;

    uint32_t timer_cycles = increase_counter(
        timer3_divider_cycle, cycles, timer3_divider);
    if(timer_cycles == 0) return;

    uint32_t prr1 = data[0x65];
    constexpr uint32_t prtim3 = 1 << 3;
    if(prr1 & prtim3) return;

    cycle_timer1_or_timer3(*this, timer_cycles,
        0x90, 0x91, 0x92,
        0x38, 0x96,
        0x98, 0x9a, 0x9c,
        0x94,
        timer3_count_down,
        timer3_phase_correct,
        timer3_top,
        timer3_tov);
}

}
