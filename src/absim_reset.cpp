#include "absim.hpp"

namespace absim
{

void atmega32u4_t::reset()
{
    // clear all registers and RAM, reset state

    // preserve button pins
    uint8_t pinb = data[0x23];
    uint8_t pine = data[0x2c];
    uint8_t pinf = data[0x2f];

    memset(&data[0], 0, 32 + 64 + 160 + 2560);

    data[0x23] = pinb;
    data[0x2c] = pine;
    data[0x2f] = pinf;

    pc = 0;

    just_read = 0;
    just_written = 0;

    active = true;
    wakeup_cycles = false;

    memset(&eeprom, 0xff, sizeof(eeprom));

    prev_sreg = 0;

    cycles_till_next_instr = 0;

    timer0_divider_cycle = 0;

    pll_lock_cycle = 0;

    spsr_read_after_transmit = false;
    spi_busy = false;
    spi_done = false;
    spi_data_byte = 0;
    spi_clock_cycle = 0;
    spi_bit_progress = 0;

    eeprom_clear_eempe_cycles = 0;
    eeprom_write_addr = 0;
    eeprom_write_data = 0;
    eeprom_program_cycles = 0;
}

}
