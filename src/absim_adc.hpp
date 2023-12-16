#include "absim.hpp"

// TODO: auto trigger (ADATE bit in ADCSRA, ADTS bits in ADCSRB)

namespace absim
{

void atmega32u4_t::adc_handle_prr0(uint8_t x)
{
    constexpr uint8_t pradc = 1 << 0;
    if(x & pradc)
        adc_busy = false;
    else
    {
        uint8_t adcsra = data[0x7a];
        adc_busy = (adcsra & 0xc0) != 0;
    }
}

void atmega32u4_t::adc_st_handle_adcsra(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == 0x7a);
    constexpr uint8_t pradc = 1 << 0;
    if(cpu.data[0x64] & pradc)
        return;
    if(x & 0xc0)
        cpu.adc_busy = true;
    cpu.data[0x7a] = x;
}

ARDENS_FORCEINLINE void atmega32u4_t::cycle_adc(uint32_t cycles)
{
    if(!adc_busy)
        return;

	uint8_t& adcsra = data[0x7a];
	uint32_t adps  = adcsra & 0x7;

	uint32_t prescaler = 1 << adps;
	if(prescaler <= 1)
		prescaler = 2;

    uint32_t tcycles = increase_counter(adc_prescaler_cycle, cycles, prescaler);
    if(tcycles == 0) return;

	uint32_t adcsrb = data[0x7b];
	uint32_t admux = data[0x7c];

	uint32_t adts = adcsrb & 0xf;
	uint32_t mux = (admux & 0x1f) | (adcsrb & 0x20);
    uint32_t refs = admux >> 6;

	for(uint32_t i = 0; i < tcycles; ++i)
	{
		if(++adc_cycle < 13)
			continue;
		adc_cycle = 0;

		constexpr uint8_t ADSC = 1 << 6;
		adcsra &= ~ADSC;
        
        if(mux == 0x27)
        {
            // temperature sensor
            adc_result = 302;
        }
        else if(mux == 0x1e)
        {
            // voltage with bandgap ref
            adc_result = 265;
        }
        else
            adc_result = 100;

        {
            uint32_t x = adc_seed;
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            adc_seed = x;
            adc_result += (int(adc_seed & 15) - 8);
        }

        adc_busy = false;
        break;
	}

	uint32_t adc = adc_result;

	// ADLAR
	if(admux & 0x20)
		adc <<= 6;

	uint8_t& adcl = data[0x78];
	uint8_t& adch = data[0x79];
	adcl = uint8_t(adc >> 0);
	adch = uint8_t(adc >> 8);
}

}
