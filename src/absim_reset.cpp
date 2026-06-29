#include "absim.hpp"

#include <random>

namespace absim
{

void atmega32u4_t::reset()
{
    cycle_count = 0;

    soft_reset();

    // power-on reset flag
    data[reg::addr::MCUSR] |= reg::bit::MCUSR::PORF;

    for(auto& byte : eeprom) byte = 0xff;

    eeprom_modified_bytes.reset();
    eeprom_modified = false;
    eeprom_dirty = false;

    serial_bytes.clear();

    for(auto& h : ld_handlers) h = nullptr;
    for(auto& h : st_handlers) h = nullptr;

    adc_seed = 0xcafebabe;
    if(adc_nondeterminism)
        adc_seed = (uint32_t)std::random_device{}();

    sound_buffer.clear();

    st_handlers[reg::addr::EECR] = eeprom_handle_st_eecr;
    st_handlers[reg::addr::PLLCSR] = pll_handle_st_pllcsr;
    st_handlers[reg::addr::SPCR] = spi_handle_st_spcr_or_spsr;
    st_handlers[reg::addr::SPSR] = spi_handle_st_spcr_or_spsr;
    st_handlers[reg::addr::SPDR] = spi_handle_st_spdr;
    st_handlers[reg::addr::PRR0] = st_handle_prr0;
    st_handlers[reg::addr::PRR1] = st_handle_prr1;
    st_handlers[reg::addr::GTCCR] = st_handle_gtccr;
    st_handlers[reg::addr::ADCSRA] = adc_st_handle_adcsra;

    st_handlers[reg::addr::PINB] = st_handle_pin;
    st_handlers[reg::addr::PINC] = st_handle_pin;
    st_handlers[reg::addr::PIND] = st_handle_pin;
    st_handlers[reg::addr::PINE] = st_handle_pin;
    st_handlers[reg::addr::PINF] = st_handle_pin;

    //st_handlers[reg::addr::PORTB] = st_handle_port;
    st_handlers[reg::addr::PORTC] = st_handle_port;
    //st_handlers[reg::addr::PORTD] = st_handle_port;
    //st_handlers[reg::addr::PORTE] = st_handle_port;
    //st_handlers[reg::addr::PORTF] = st_handle_port;

    for(int i = reg::addr::TCCR0A; i <= reg::addr::OCR0B; ++i)
        st_handlers[i] = timer0_handle_st_regs;
    st_handlers[reg::addr::TCNT0] = timer0_handle_st_tcnt;

    for(int i = reg::addr::TCCR1A; i <= reg::addr::OCR1CH; ++i)
        st_handlers[i] = timer1_handle_st_regs;
    for(int i = reg::addr::TCCR3A; i <= reg::addr::OCR3CH; ++i)
        st_handlers[i] = timer3_handle_st_regs;
    for(int i = reg::addr::TCNT4; i <= reg::addr::TCCR4E; ++i)
        st_handlers[i] = timer4_handle_st_regs;
    for(int i = reg::addr::OCR4A; i <= reg::addr::OCR4D; ++i)
        st_handlers[i] = timer4_handle_st_ocrN;

    // TIMSK4
    st_handlers[reg::addr::TIMSK4] = timer4_handle_st_regs;

    st_handlers[reg::addr::TIFR0] = timer0_handle_st_tifr;
    st_handlers[reg::addr::TIFR1] = timer1_handle_st_tifr;
    st_handlers[reg::addr::TIFR3] = timer3_handle_st_tifr;
    st_handlers[reg::addr::TIFR4] = timer4_handle_st_tifr;

    st_handlers[reg::addr::DDRC] = sound_st_handler_ddrc;

    ld_handlers[reg::addr::SPSR] = spi_handle_ld_spsr;
    ld_handlers[reg::addr::SPDR] = spi_handle_ld_spdr;
    ld_handlers[reg::addr::TCNT0] = timer0_handle_ld_tcnt;

    st_handlers[reg::addr::MCUSR] = st_handle_mcusr;
    st_handlers[reg::addr::MCUCR] = st_handle_mcucr;
    st_handlers[reg::addr::SPMCSR] = st_handle_spmcsr;
    st_handlers[reg::addr::WDTCSR] = st_handle_wdtcsr;

    st_handlers[reg::addr::TIMSK0] = st_handler_timsk;
    st_handlers[reg::addr::TIMSK1] = st_handler_timsk;
    st_handlers[reg::addr::TIMSK3] = st_handler_timsk;
    st_handlers[reg::addr::TIMSK4] = st_handler_timsk;

    for(int i = reg::addr::TCNT1L; i <= reg::addr::OCR1CH; ++i)
        ld_handlers[i] = timer1_handle_ld_regs;
    for(int i = reg::addr::TCNT3L; i <= reg::addr::OCR3CH; ++i)
        ld_handlers[i] = timer3_handle_ld_regs;
    ld_handlers[reg::addr::TCNT4] = timer4_handle_ld_tcnt;
    ld_handlers[reg::addr::TC4H] = timer4_handle_ld_tcnt;

    st_handlers[reg::addr::UHWCON] = usb_st_handler;
    st_handlers[reg::addr::USBCON] = usb_st_handler;
    st_handlers[reg::addr::USBSTA] = usb_st_handler;
    st_handlers[reg::addr::USBINT] = usb_st_handler;
    for(int i = reg::addr::UDCON; i <= reg::addr::UDMFN; ++i)
        st_handlers[i] = usb_st_handler;
    for(int i = reg::addr::UEINTX; i <= reg::addr::UEINT; ++i)
        st_handlers[i] = usb_st_handler;

    ld_handlers[reg::addr::UEDATX] = usb_ld_handler_uedatx;
}

void atmega32u4_t::soft_reset()
{
    // clear all registers and RAM, reset state

    // preserve button pins
    uint8_t pinb = data[reg::addr::PINB];
    uint8_t pine = data[reg::addr::PINE];
    uint8_t pinf = data[reg::addr::PINF];

    data = {};

    data[reg::addr::PINB] = pinb;
    data[reg::addr::PINE] = pine;
    data[reg::addr::PINF] = pinf;

    // turn off TX/RX LEDs at reset (assume bootloader has turned them off)
    data[reg::addr::PORTB] |= reg::bit::PORTB::PORTB0;
    data[reg::addr::PORTD] |= reg::bit::PORTD::PORTD5;

    data[reg::addr::PLLFRQ] = reg::bit::PLLFRQ::PDIV2;

    pc = BOOTRST() ? bootloader_address() : 0;
    executing_instr_pc = pc;

    just_read = 0xffffffff;
    just_written = 0xffffffff;

    active = true;
    wakeup_cycles = false;
    just_interrupted = false;

    num_stack_frames = 0;
    pushed_at_least_once = false;
    autobreaks.reset();

    prev_sreg = 0;

    timer0 = {};
    timer1 = {};
    timer3 = {};
    timer4 = {};
    timer_sync = {};

    data[reg::addr::OCR4C] = timer4.ocrNc = timer4.ocrNc_next = timer4.top = 0xff;

    timer1.base_addr = reg::addr::TCCR1A;
    timer3.base_addr = reg::addr::TCCR3A;
    timer1.tifrN_addr = reg::addr::TIFR1;
    timer3.tifrN_addr = reg::addr::TIFR3;
    timer1.timskN_addr = reg::addr::TIMSK1;
    timer3.timskN_addr = reg::addr::TIMSK3;
    timer1.prr_addr = reg::addr::PRR0;
    timer3.prr_addr = reg::addr::PRR1;
    timer1.prr_mask = reg::bit::PRR0::PRTIM1;
    timer3.prr_mask = reg::bit::PRR1::PRTIM3;

    timer_sync.prev_update_cycle = cycle_count;
    timer4.prev_update_cycle = cycle_count;

    timer_sync.next_update_cycle = UINT64_MAX;
    timer0.next_update_cycle = UINT64_MAX;
    timer1.next_update_cycle = UINT64_MAX;
    timer3.next_update_cycle = UINT64_MAX;
    timer4.next_update_cycle = UINT64_MAX;

    pll_prev_cycle = cycle_count;
    pll_lock_cycle = 0;
    pll_num12 = 0;
    pll_busy = false;

    spsr_read_after_transmit = false;
    spi_busy = false;
    spi_busy_clear = false;
    spi_latch_read = false;
    spi_data_latched = false;
    spi_data_byte = 0;
    spi_datain_byte = 0;
    spi_clock_cycles = 4;
    spi_done_cycle = UINT64_MAX;
    spi_transmit_zero_cycle = UINT64_MAX;

    eeprom_prev_cycle = cycle_count;
    eeprom_clear_eempe_cycles = 0;
    eeprom_write_addr = 0;
    eeprom_write_data = 0;
    eeprom_program_cycles = 0;
    eeprom_busy = false;

    adc_prev_cycle = cycle_count;
    adc_prescaler_cycle = 0;
    adc_cycle = 0;
    adc_ref = 0;
    adc_result = 0;
    adc_busy = false;

    sound_prev_cycle = cycle_count;
    sound_cycle = 0;
    sound_enabled = 0;
    sound_pwm = false;
    sound_pwm_val = 0;

    min_stack = 0xffff;
    pushed_at_least_once = false;

    reset_usb();
    usb_dpram = {};

    spm_prev_cycle = cycle_count;
    spm_busy = false;
    spm_op = SPM_OP_NONE;
    spm_cycles = 0;
    erase_spm_buffer();

    watchdog_divider = 0;
    watchdog_divider_cycle = 0;
    watchdog_prev_cycle = cycle_count;
    update_watchdog_prescaler();
    watchdog_next_cycle = cycle_count + watchdog_divider;
    peripheral_queue.schedule(watchdog_next_cycle, PQ_WATCHDOG);

    data[reg::addr::OSCCAL] = 0x6d;

    peripheral_queue.clear();
}

}
