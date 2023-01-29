#include "absim.hpp"

// TODO: auto trigger (ADATE bit in ADCSRA, ADTS bits in ADCSRB)

namespace absim
{

FORCEINLINE void atmega32u4_t::cycle_adc(uint32_t cycles)
{
	// check if adc is powered down
	uint32_t prr0 = data[0x64];
	constexpr uint32_t pradc = 1 << 0;
	if(prr0 & pradc)
		return;

	uint8_t& adcsra = data[0x7a];

	// adc enable turned off or not converting
	if((adcsra & 0xc0) == 0)
	{
		adc_cycle = 0;
		adc_prescaler_cycle = 0;
		return;
	}

	uint32_t adps  = adcsra & 0x7;

	uint32_t prescaler = 1 << adps;
	if(prescaler <= 1)
		prescaler = 2;

	if(++adc_prescaler_cycle < prescaler)
		return;
	adc_prescaler_cycle = 0;

	uint8_t& adcsrb = data[0x7b];
	uint8_t& admux = data[0x7c];

	uint32_t adts = adcsrb & 0xf;
	uint32_t mux = admux & 0x1f;

	for(uint32_t i = 0; i < cycles; ++i)
	{
		if(++adc_cycle < 13)
			continue;
		adc_cycle = 0;

		constexpr uint8_t ADSC = 1 << 6;
		adcsra &= ~ADSC;

		adc_result = 500;
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
