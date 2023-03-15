#include "absim.hpp"

namespace absim
{

uint8_t atmega32u4_t::led_tx() const
{
    uint8_t portd = data[0x2b];
    return (portd & (1 << 5)) ? 0 : 255;
}

uint8_t atmega32u4_t::led_rx() const
{
    uint8_t portb = data[0x25];
    return (portb & (1 << 0)) ? 0 : 255;
}

void atmega32u4_t::led_rgb(uint8_t& r, uint8_t& g, uint8_t& b) const
{
    // TODO: accurate modeling of PWM

    uint8_t t;
    r = g = b = 0;

    if((data[0x44] & 0xc3) == 0x83) // tccr0a
        g = 255 - data[0x47]; // ocr4a

    if((data[0x80] & 0xf3) == 0xf1 && (data[0x81] & 0x18) == 0) // tccr1[a,b]
    {
        r = data[0x8a];
        b = data[0x88];
    }
}

}
