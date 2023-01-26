#include "absim.hpp"

namespace absim
{

void atmega32u4_t::cycle_timer0()
{
    uint16_t divider = 0;
    bool external = false;
    bool rising = false;
    uint8_t cs = tccr0b() & 0x7;
    uint8_t wgm = (tccr0a() & 0x3) | ((tccr0b() >> 1) & 0x4);
    switch(cs)
    {
    case 0: divider = 0; break;
    case 1: divider = 1; break;
    case 2: divider = 8; break;
    case 3: divider = 64; break;
    case 4: divider = 256; break;
    case 5: divider = 1024; break;
    case 6: external = true; rising = false; break;
    case 7: external = true; rising = true; break;
    default: break;
    }

    // Arduboy has no external clock
    if(external)
        return;

    if(divider == 0)
        return;

    if(++timer0_divider_cycle < divider)
        return;

    uint8_t& t = tcnt0();
    timer0_divider_cycle = 0;
    ++t;

    if(t == 0)
        tifr0() |= (1 << 0);
    else if(t == ocr0a())
        t = 0, tifr0() |= (1 << 1);
    else if(t == ocr0b())
        t = 0, tifr0() |= (1 << 2);
}

}
