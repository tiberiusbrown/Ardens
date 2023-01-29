#include "absim.hpp"

namespace absim
{

static FORCEINLINE uint16_t word(atmega32u4_t const& cpu, int addr)
{
    uint16_t lo = cpu.data[addr + 0];
    uint16_t hi = cpu.data[addr + 1];
    return lo | (hi << 8);
}

static FORCEINLINE int get_divider(int cs)
{
    assert((cs & ~7) == 0);
    static constexpr uint16_t DIVIDERS[8] =
    {
        0, 1, 8, 64, 256, 1024, 0, 0,
    };
    return DIVIDERS[cs];
}

void FORCEINLINE atmega32u4_t::cycle_timer0()
{
    int cs = tccr0b() & 0x7;
    int divider = get_divider(cs);
    int wgm = (tccr0a() & 0x3) | ((tccr0b() >> 1) & 0x4);

    // writing a logic 1 to TOV0 clears it
    if(just_written == 0x35 && (tifr0() & 0x1))
        tifr0() &= ~0x1;

    // timer0 is powered down
    int prr0 = data[0x64];
    constexpr int prtim0 = 1 << 5;
    if(prr0 & prtim0)
        return;

    // invalid prescaler setting
    if(divider == 0)
        return;

    // wait for prescaler
    if(++timer0_divider_cycle < divider)
        return;
    timer0_divider_cycle = 0;

    uint8_t top = 0xff;
    uint8_t tov = 0xff;

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

// TODO: cache this stuff
static FORCEINLINE void process_wgm16(
    uint8_t wgm, uint16_t& top, uint16_t& tov, uint16_t ocr, uint16_t icr)
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

static inline FORCEINLINE void cycle_timer1_or_timer3(atmega32u4_t& cpu,
    uint8_t   tccrNa_addr,
    uint8_t   tccrNb_addr,
    uint8_t   tccrNc_addr,
    uint8_t   tifrN_addr,
    uint8_t   icrN_addr,
    uint8_t   ocrNa_addr,
    uint8_t   ocrNb_addr,
    uint8_t   ocrNc_addr,
    uint8_t   tcntN_addr,
    bool&     count_down,
    bool      phase_correct,
    uint16_t  top,
    uint16_t  tov
    )
{
    uint8_t& tifrN = cpu.data[tifrN_addr];

    // writing a logic 1 to TOV1 clears it
    if(cpu.just_written == tifrN_addr && (tifrN & 0x1))
        tifrN &= ~0x1;

    uint16_t ocrNa = word(cpu, ocrNa_addr);
    uint16_t ocrNb = word(cpu, ocrNb_addr);
    uint16_t ocrNc = word(cpu, ocrNc_addr);

    uint16_t t = word(cpu, tcntN_addr);

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

    cpu.data[tcntN_addr + 0] = uint8_t(t >> 0);
    cpu.data[tcntN_addr + 1] = uint8_t(t >> 8);
}

void FORCEINLINE atmega32u4_t::cycle_timer1()
{
    if(just_written == 0x81)
    {
        int cs = data[0x81] & 0x7;
        timer1_divider = get_divider(cs);

        uint16_t icrN = word(*this, 0x86);
        uint16_t ocrNa = word(*this, 0x88);
        uint8_t tccrNa = data[0x80];
        uint8_t tccrNb = data[0x81];
        uint8_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);
        process_wgm16(wgm, timer1_top, timer1_tov, ocrNa, icrN);
        timer1_phase_correct = ((wgm & 0x3) == 1);
        if(!timer1_phase_correct)
            timer1_count_down = false;
    }

    if(++timer1_divider_cycle < timer1_divider)
        return;
    timer1_divider_cycle = 0;

    uint8_t prr0 = data[0x64];
    constexpr uint8_t prtim1 = 1 << 3;
    if(prr0 & prtim1) return;

    cycle_timer1_or_timer3(*this,
        0x80, 0x81, 0x82,
        0x36, 0x86,
        0x88, 0x8a, 0x8c,
        0x84,
        timer1_count_down,
        timer1_phase_correct,
        timer1_top,
        timer1_tov);
}

void FORCEINLINE atmega32u4_t::cycle_timer3()
{
    if(just_written == 0x91)
    {
        int cs = data[0x91] & 0x7;
        timer3_divider = get_divider(cs);

        uint16_t icrN = word(*this, 0x96);
        uint16_t ocrNa = word(*this, 0x98);
        uint8_t tccrNa = data[0x90];
        uint8_t tccrNb = data[0x91];
        uint8_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);
        process_wgm16(wgm, timer3_top, timer3_tov, ocrNa, icrN);
        timer3_phase_correct = ((wgm & 0x3) == 1);
        if(!timer3_phase_correct)
            timer3_count_down = false;
    }

    if(++timer3_divider_cycle < timer3_divider)
        return;
    timer3_divider_cycle = 0;

    uint8_t prr1 = data[0x65];
    constexpr uint8_t prtim3 = 1 << 3;
    if(prr1 & prtim3) return;

    cycle_timer1_or_timer3(*this,
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
