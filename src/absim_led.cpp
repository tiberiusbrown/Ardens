#include "absim.hpp"

namespace absim
{

uint8_t atmega32u4_t::led_tx() const
{
    uint8_t ddrd = data[reg::addr::DDRD];
    if(!(ddrd & reg::bit::DDRD::DDD5)) return 0;
    uint8_t portd = data[reg::addr::PORTD];
    return (portd & reg::bit::PORTD::PORTD5) ? 0 : 255;
}

uint8_t atmega32u4_t::led_rx() const
{
    uint8_t ddrb = data[reg::addr::DDRB];
    if(!(ddrb & reg::bit::DDRB::DDB0)) return 0;
    uint8_t portb = data[reg::addr::PORTB];
    return (portb & reg::bit::PORTB::PORTB0) ? 0 : 255;
}

void atmega32u4_t::led_rgb(uint8_t& r, uint8_t& g, uint8_t& b) const
{
    // TODO: accurate modeling of PWM

    r = g = b = 0;

    uint8_t ddrb = data[reg::addr::DDRB];
    uint8_t portb = data[reg::addr::PORTB];
    if((ddrb & reg::bit::DDRB::DDB6) && !(portb & reg::bit::PORTB::PORTB6)) r = 255;
    if((ddrb & reg::bit::DDRB::DDB7) && !(portb & reg::bit::PORTB::PORTB7)) g = 255;
    if((ddrb & reg::bit::DDRB::DDB5) && !(portb & reg::bit::PORTB::PORTB5)) b = 255;

    if(data[reg::addr::TCCR0A] ==
        (reg::bit::TCCR0A::COM0A1 | reg::bit::TCCR0A::WGM01 | reg::bit::TCCR0A::WGM00))
        g = 255 - data[reg::addr::OCR0A];

    if(data[reg::addr::TCCR1A] ==
        (reg::bit::TCCR1A::COM1A1 | reg::bit::TCCR1A::COM1A0 |
            reg::bit::TCCR1A::COM1B1 | reg::bit::TCCR1A::COM1B0 |
            reg::bit::TCCR1A::WGM10) &&
        (data[reg::addr::TCCR1B] &
            (reg::bit::TCCR1B::WGM13 | reg::bit::TCCR1B::WGM12)) == 0)
    {
        r = data[reg::addr::OCR1BL];
        b = data[reg::addr::OCR1AL];
    }
}

}
