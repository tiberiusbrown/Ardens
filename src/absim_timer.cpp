#include "absim.hpp"

namespace absim
{

static uint16_t word(atmega32u4_t const& cpu, uint16_t addr)
{
    uint16_t lo = cpu.data[addr + 0];
    uint16_t hi = cpu.data[addr + 1];
    return lo | (hi << 8);
}

static uint16_t get_divider(uint8_t cs, bool& rising, bool& external)
{
    uint16_t r = 0;
    external = false;
    rising = false;
    switch(cs)
    {
    case 0: r = 0; break;
    case 1: r = 1; break;
    case 2: r = 8; break;
    case 3: r = 64; break;
    case 4: r = 256; break;
    case 5: r = 1024; break;
    case 6: external = true; rising = false; break;
    case 7: external = true; rising = true; break;
    default: break;
    }
    return r;
}

void atmega32u4_t::cycle_timer0()
{
    bool rising;
    bool external;
    uint8_t cs = tccr0b() & 0x7;
    uint16_t divider = get_divider(cs, rising, external);
    uint8_t wgm = (tccr0a() & 0x3) | ((tccr0b() >> 1) & 0x4);

    // writing a logic 1 to TOV0 clears it
    if(just_written == 0x35 && (tifr0() & 0x1))
        tifr0() &= ~0x1;

    // timer0 is powered down
    uint8_t prr0 = data[0x64];
    constexpr uint8_t prtim0 = 1 << 5;
    if(prr0 & prtim0)
        return;

    // Arduboy has no external clock so timer will never advance
    if(external)
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
static void process_wgm16(
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

struct timer1_or_timer3_info_t
{
    uint8_t   tccrNa_addr;
    uint8_t   tccrNb_addr;
    uint8_t   tccrNc_addr;
    uint8_t   tifrN_addr;
    bool      powered_down;
    uint8_t   icrN_addr;
    uint8_t   ocrNa_addr;
    uint8_t   ocrNb_addr;
    uint8_t   ocrNc_addr;
    uint8_t   tcntN_addr;
    uint16_t* divider_cycle;
    bool*     count_down;
};

static void cycle_timer1_or_timer3(atmega32u4_t& cpu, timer1_or_timer3_info_t& info)
{
    uint8_t tccrNb = cpu.data[info.tccrNb_addr];
    uint8_t cs = tccrNb & 0x7;
    bool rising;
    bool external;
    uint16_t divider = get_divider(cs, rising, external);

    uint8_t tccrNa = cpu.data[info.tccrNa_addr];
    uint8_t tccrNc = cpu.data[info.tccrNc_addr];

    // invalid prescaler setting
    if(divider == 0)
        return;

    uint8_t& tifrN = cpu.data[info.tifrN_addr];

    // writing a logic 1 to TOV1 clears it
    if(cpu.just_written == info.tifrN_addr && (tifrN & 0x1))
        tifrN &= ~0x1;

    // timer powered down
    if(info.powered_down)
        return;

    // Arduboy has no external clock so timer will never advance
    if(external)
        return;

    // wait for prescaler
    if(++(*info.divider_cycle) < divider)
        return;
    *info.divider_cycle = 0;

    uint16_t top, tov;
    uint8_t wgm = (tccrNa & 0x3) | ((tccrNb >> 1) & 0xc);

    uint16_t icrN  = word(cpu, info.icrN_addr);
    uint16_t ocrNa = word(cpu, info.ocrNa_addr);
    uint16_t ocrNb = word(cpu, info.ocrNb_addr);
    uint16_t ocrNc = word(cpu, info.ocrNc_addr);

    process_wgm16(wgm, top, tov, ocrNa, icrN);

    uint16_t t = word(cpu, info.tcntN_addr);

    if(t == tov)
        tifrN |= (1 << 0);
    if(t == ocrNa)
        tifrN |= (1 << 1);
    if(t == ocrNb)
        tifrN |= (1 << 2);
    if(t == ocrNc)
        tifrN |= (1 << 3);

    auto& timer_count_down = *info.count_down;
    if((wgm & 0x3) != 1)
        timer_count_down = false;
    if(t == top)
    {
        if((wgm & 0x3) == 1)
        {
            timer_count_down = !timer_count_down;
            t = timer_count_down ? t - 1 : t + 1;
        }
        else
            t = 0;
    }
    else
    {
        t = timer_count_down ? t - 1 : t + 1;
    }

    cpu.data[info.tcntN_addr + 0] = uint8_t(t >> 0);
    cpu.data[info.tcntN_addr + 1] = uint8_t(t >> 8);
}

void atmega32u4_t::cycle_timer1()
{
    uint8_t prr0 = data[0x64];
    constexpr uint8_t prtim1 = 1 << 3;

    timer1_or_timer3_info_t info;
    info.tccrNa_addr = 0x80;
    info.tccrNb_addr = 0x81;
    info.tccrNc_addr = 0x82;
    info.tifrN_addr = 0x36;
    info.powered_down = (prr0 & prtim1) != 0;
    info.icrN_addr = 0x86;
    info.ocrNa_addr = 0x88;
    info.ocrNb_addr = 0x8a;
    info.ocrNc_addr = 0x8c;
    info.tcntN_addr = 0x84;
    info.divider_cycle = &timer1_divider_cycle;
    info.count_down = &timer1_count_down;
    cycle_timer1_or_timer3(*this, info);
}

void atmega32u4_t::cycle_timer3()
{
    uint8_t prr1 = data[0x65];
    constexpr uint8_t prtim3 = 1 << 3;

    timer1_or_timer3_info_t info;
    info.tccrNa_addr = 0x90;
    info.tccrNb_addr = 0x91;
    info.tccrNc_addr = 0x92;
    info.tifrN_addr = 0x38;
    info.powered_down = (prr1 & prtim3) != 0;
    info.icrN_addr = 0x96;
    info.ocrNa_addr = 0x98;
    info.ocrNb_addr = 0x9a;
    info.ocrNc_addr = 0x9c;
    info.tcntN_addr = 0x94;
    info.divider_cycle = &timer3_divider_cycle;
    info.count_down = &timer3_count_down;
    cycle_timer1_or_timer3(*this, info);
}

}
