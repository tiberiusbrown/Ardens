#include "absim.hpp"

namespace absim
{

void atmega32u4_t::sound_st_handler_ddrc(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    if(ptr == 0x27)
    {
        // DDRC
        uint32_t e = 0;
        if(x & (1 << 6))
            e |= (1 << 0);
        if(x & (1 << 7))
            e |= (1 << 1);
        cpu.sound_enabled = e;
    }
    cpu.data[ptr] = x;
}
    
ARDENS_FORCEINLINE void atmega32u4_t::cycle_sound(uint32_t cycles)
{
    uint32_t samples = increase_counter(sound_cycle, cycles, SOUND_CYCLES);
    if(samples == 0)
        return;

    auto pins = sound_enabled;

    if(pins == 0)
    { 
        for(uint32_t i = 0; i < samples; ++i)
            sound_buffer.push_back(0);
        return;
    }

    int16_t x = sound_pwm_val;
    if(!sound_pwm)
    {
        x = 0;
        uint8_t portc = data[0x28];
        if(pins & (1 << 0))
            x += (portc & (1 << 6)) ? SOUND_GAIN / 2 : -SOUND_GAIN / 2;
        if(pins & (1 << 1))
            x += (portc & (1 << 7)) ? -SOUND_GAIN / 2 : SOUND_GAIN / 2;
    }
    for(uint32_t i = 0; i < samples; ++i)
    {
        sound_buffer.push_back(x);
    }
}
    
}
