#include "absim.hpp"

#include <random>

namespace absim
{

void atmega32u4_t::reset()
{
    // clear all registers and RAM, reset state

    // preserve button pins
    uint8_t pinb = data[0x23];
    uint8_t pine = data[0x2c];
    uint8_t pinf = data[0x2f];

    data = {};

    data[0x23] = pinb;
    data[0x2c] = pine;
    data[0x2f] = pinf;

    // turn off TX/RX LEDs at reset (assume bootloader has turned them off)
    data[0x25] |= 0x01;
    data[0x2b] |= 0x20;

    pc = 0;
    executing_instr_pc = 0;

    just_read = 0xffffffff;
    just_written = 0xffffffff;

    active = true;
    wakeup_cycles = false;
    just_interrupted = false;

    num_stack_frames = 0;
    pushed_at_least_once = false;
    autobreaks.reset();

    for(auto& byte : eeprom) byte = 0xff;

    prev_sreg = 0;

    for(auto& h : ld_handlers) h = nullptr;
    for(auto& h : st_handlers) h = nullptr;

    st_handlers[0x3f] = eeprom_handle_st_eecr;
    st_handlers[0x49] = pll_handle_st_pllcsr;
    st_handlers[0x4c] = spi_handle_st_spcr_or_spsr;
    st_handlers[0x4d] = spi_handle_st_spcr_or_spsr;
    st_handlers[0x4e] = spi_handle_st_spdr;
    st_handlers[0x64] = st_handle_prr0;
    st_handlers[0x7a] = adc_st_handle_adcsra;

    st_handlers[0x23] = st_handle_pin;
    st_handlers[0x26] = st_handle_pin;
    st_handlers[0x29] = st_handle_pin;
    st_handlers[0x2c] = st_handle_pin;
    st_handlers[0x2f] = st_handle_pin;

    //st_handlers[0x25] = st_handle_port;
    //st_handlers[0x28] = st_handle_port;
    //st_handlers[0x2b] = st_handle_port;
    //st_handlers[0x2e] = st_handle_port;
    //st_handlers[0x31] = st_handle_port;

    for(int i = 0x44; i <= 0x48; ++i)
        st_handlers[i] = timer0_handle_st_regs;
    st_handlers[0x46] = timer0_handle_st_tcnt;
    for(int i = 0x80; i <= 0x8d; ++i)
        st_handlers[i] = timer1_handle_st_regs;
    for(int i = 0x90; i <= 0x9d; ++i)
        st_handlers[i] = timer3_handle_st_regs;
    for(int i = 0xbe; i <= 0xc4; ++i)
        st_handlers[i] = timer4_handle_st_regs;
    for(int i = 0xcf; i <= 0xd2; ++i)
        st_handlers[i] = timer4_handle_st_ocrN;

    // TIMSK4
    st_handlers[0x72] = timer4_handle_st_regs;

    st_handlers[0x35] = timer0_handle_st_tifr;
    st_handlers[0x36] = timer1_handle_st_tifr;
    st_handlers[0x38] = timer3_handle_st_tifr;
    st_handlers[0x39] = timer4_handle_st_tifr;

    st_handlers[0x27] = sound_st_handler_ddrc;

    ld_handlers[0x4d] = spi_handle_ld_spsr;
    ld_handlers[0x4e] = spi_handle_ld_spdr;
    ld_handlers[0x46] = timer0_handle_ld_tcnt;
    for(int i = 0x84; i <= 0x8d; ++i)
        ld_handlers[i] = timer1_handle_ld_regs;
    for(int i = 0x94; i <= 0x9d; ++i)
        ld_handlers[i] = timer3_handle_ld_regs;
    ld_handlers[0xbe] = timer4_handle_ld_tcnt;
    ld_handlers[0xbf] = timer4_handle_ld_tcnt;

    st_handlers[0xd7] = usb_st_handler;
    st_handlers[0xd8] = usb_st_handler;
    st_handlers[0xd9] = usb_st_handler;
    st_handlers[0xda] = usb_st_handler;
    for(int i = 0xe0; i <= 0xe6; ++i)
        st_handlers[i] = usb_st_handler;
    for(int i = 0xe8; i <= 0xf4; ++i)
        st_handlers[i] = usb_st_handler;

    ld_handlers[0xf1] = usb_ld_handler_uedatx;

    timer0 = {};
    timer1 = {};
    timer3 = {};
    timer4 = {};

    data[0xd1] = timer4.ocrNc = timer4.ocrNc_next = timer4.top = 0xff;

    timer1.base_addr = 0x80;
    timer3.base_addr = 0x90;
    timer1.tifrN_addr = 0x36;
    timer3.tifrN_addr = 0x38;
    timer1.timskN_addr = 0x6f;
    timer3.timskN_addr = 0x71;
    timer1.prr_addr = 0x64;
    timer3.prr_addr = 0x65;
    timer1.prr_mask = 1 << 3;
    timer3.prr_mask = 1 << 3;

    timer0.next_update_cycle = UINT64_MAX;
    timer1.next_update_cycle = UINT64_MAX;
    timer3.next_update_cycle = UINT64_MAX;
    timer4.next_update_cycle = UINT64_MAX;

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

    eeprom_clear_eempe_cycles = 0;
    eeprom_write_addr = 0;
    eeprom_write_data = 0;
    eeprom_program_cycles = 0;
    eeprom_busy = false;

    adc_prescaler_cycle = 0;
    adc_cycle = 0;
    adc_ref = 0;
    adc_result = 0;
    adc_busy = false;
    adc_seed = 0xcafebabe;
    if(adc_nondeterminism)
        adc_seed = (uint32_t)std::random_device{}();

    sound_cycle = 0;
    sound_enabled = 0;
    sound_pwm = false;
    sound_pwm_val = 0;
    sound_buffer.clear();

    cycle_count = 0;

    min_stack = 0xffff;
    pushed_at_least_once = false;

    serial_bytes.clear();
    reset_usb();
    usb_dpram = {};

    eeprom_modified_bytes.reset();
    eeprom_modified = false;
    eeprom_dirty = false;
}

}
