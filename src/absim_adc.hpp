#include "absim.hpp"

// TODO: auto trigger (ADATE bit in ADCSRA, ADTS bits in ADCSRB)

namespace absim
{

void atmega32u4_t::adc_handle_prr0(uint8_t x)
{
    constexpr uint8_t pradc = reg::bit::PRR0::PRADC;
    if(x & pradc)
        adc_busy = false;
    else
    {
        uint8_t adcsra = data[reg::addr::ADCSRA];
        adc_busy = (adcsra & (reg::bit::ADCSRA::ADEN | reg::bit::ADCSRA::ADSC)) != 0;
    }
    adc_schedule();
}

void atmega32u4_t::adc_st_handle_adcsra(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::ADCSRA);
    constexpr uint8_t pradc = reg::bit::PRR0::PRADC;
    if(cpu.data[reg::addr::PRR0] & pradc)
        return;
    if(x & (reg::bit::ADCSRA::ADEN | reg::bit::ADCSRA::ADSC))
        cpu.adc_busy = true;
    cpu.data[reg::addr::ADCSRA] = x;
    cpu.adc_schedule();
}

void atmega32u4_t::adc_schedule()
{
    if(adc_busy)
    {
        uint32_t adps = data[reg::addr::ADCSRA] &
            (reg::bit::ADCSRA::ADPS0 | reg::bit::ADCSRA::ADPS1 | reg::bit::ADCSRA::ADPS2);
        uint32_t prescaler = 1 << adps;
        if(prescaler <= 1)
            prescaler = 2;
        uint32_t cycles = 0;
        cycles += prescaler * (13 - adc_cycle);
        if(adc_prescaler_cycle < prescaler)
            cycles += prescaler - adc_prescaler_cycle;
        if(cycles == 0) cycles = 1;
        peripheral_queue.schedule(cycle_count + cycles, PQ_ADC);
    }
}

ARDENS_FORCEINLINE void atmega32u4_t::update_adc()
{
    if(!adc_busy)
        return;

    uint32_t cycles = uint32_t(cycle_count - adc_prev_cycle);
    adc_prev_cycle = cycle_count;

	uint8_t& adcsra = data[reg::addr::ADCSRA];
	uint32_t adps  = adcsra &
        (reg::bit::ADCSRA::ADPS0 | reg::bit::ADCSRA::ADPS1 | reg::bit::ADCSRA::ADPS2);
	uint32_t prescaler = 1 << adps;
	if(prescaler <= 1)
		prescaler = 2;

    uint32_t tcycles = increase_counter(adc_prescaler_cycle, cycles, prescaler);
    if(tcycles == 0)
    {
        adc_schedule();
        return;
    }

	uint32_t adcsrb = data[reg::addr::ADCSRB];
	uint32_t admux = data[reg::addr::ADMUX];

	uint32_t mux = (admux &
        (reg::bit::ADMUX::MUX0 |
        reg::bit::ADMUX::MUX1 |
        reg::bit::ADMUX::MUX2 |
        reg::bit::ADMUX::MUX3 |
        reg::bit::ADMUX::MUX4)) |
        (adcsrb & reg::bit::ADCSRB::MUX5);

	for(uint32_t i = 0; i < tcycles; ++i)
	{
		if(++adc_cycle < 13)
			continue;
		adc_cycle = 0;

		constexpr uint8_t ADSC = reg::bit::ADCSRA::ADSC;
		adcsra &= ~ADSC;
        
        int n = 41;
        if(mux == 0x27)
        {
            // temperature sensor
            adc_result = 302;
            n = 5;
        }
        else if(mux == 0x1e)
        {
            // voltage with bandgap ref
            adc_result = 265;
        }
        else
            adc_result = 100;

        uint32_t x = adc_seed;
        for(int i = 0; i < n; ++i)
        {
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            if(x & 2)
            {
                if(x & 1)
                    ++adc_result;
                else
                    --adc_result;
            }
        }
        adc_seed = x;

        adc_busy = false;
        break;
	}

	uint32_t adc = adc_result;

	// ADLAR
	if(admux & reg::bit::ADMUX::ADLAR)
		adc <<= 6;

	uint8_t& adcl = data[reg::addr::ADCL];
	uint8_t& adch = data[reg::addr::ADCH];
	adcl = uint8_t(adc >> 0);
	adch = uint8_t(adc >> 8);

    adc_schedule();
}

}
