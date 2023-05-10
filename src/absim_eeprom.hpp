#include "absim.hpp"

namespace absim
{

void atmega32u4_t::eeprom_handle_st_eecr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == 0x3f);

    constexpr uint8_t EEMPE = (1 << 2);
    constexpr uint8_t EEPE = (1 << 1);
    constexpr uint8_t EERE = (1 << 0);

    uint8_t eecr = cpu.data[0x3f];

    if(x & EEMPE)
    {
        cpu.eeprom_clear_eempe_cycles = 5;
        cpu.eeprom_busy = true;
    }

    uint16_t eearl = cpu.data[0x41];
    uint16_t eearh = cpu.data[0x42];
    uint16_t addr = (eearl | (eearh << 8)) & 0x3ff;

    if(cpu.eeprom_program_cycles == 0 && (x & EEPE))
    {
        if(!(eecr & EEMPE))
            x &= ~EEPE;
        else
        {
            uint8_t eepm = (x >> 4) & 0x3;
            uint8_t eedr = cpu.data[0x40];

            if(eepm == 0)
            {
                // erase and write
                cpu.eeprom_program_cycles = 16 * 3400; // 3.4 ms
                cpu.eeprom_write_addr = addr;
                cpu.eeprom_write_data = eedr;
                cpu.eeprom_busy = true;
            }
            if(eepm == 1)
            {
                // erase only
                cpu.eeprom_program_cycles = 16 * 1800; // 1.8 ms
                cpu.eeprom_write_addr = addr;
                cpu.eeprom_write_data = 0xff;
                cpu.eeprom_busy = true;
            }
            if(eepm == 2)
            {
                // write only
                cpu.eeprom_program_cycles = 16 * 1800; // 1.8 ms
                cpu.eeprom_write_addr = addr;
                cpu.eeprom_write_data = eedr & cpu.eeprom[addr];
                cpu.eeprom_busy = true;
            }
        }
    }

    if(x & EERE)
    {
        x &= ~EERE;
        cpu.data[0x40] = cpu.eeprom[addr];
        cpu.wakeup_cycles = 4;
        cpu.active = false;
        cpu.eeprom_busy = false;
    }

    cpu.data[0x3f] = x;
}


void ABSIM_FORCEINLINE atmega32u4_t::cycle_eeprom(uint32_t cycles)
{
    if(!eeprom_busy)
        return;

#define eecr data[0x3f]
#define eedr data[0x40]

    constexpr uint8_t EERIE = (1 << 3);
    constexpr uint8_t EEMPE = (1 << 2);
    constexpr uint8_t EEPE  = (1 << 1);
    constexpr uint8_t EERE  = (1 << 0);

    if(eeprom_clear_eempe_cycles != 0)
    {
        if(eeprom_clear_eempe_cycles <= cycles)
        {
            eeprom_clear_eempe_cycles = 0;
            eecr &= ~EEMPE;
            if(eeprom_program_cycles == 0)
                eeprom_busy = false;
        }
        else
            eeprom_clear_eempe_cycles -= cycles;
    }

    if(eeprom_program_cycles != 0)
    {
        eecr |= EEPE;

        if(eeprom_program_cycles <= cycles)
        {
            eeprom_program_cycles = 0;
            eecr &= ~EEPE;
            eeprom[eeprom_write_addr] = eeprom_write_data;
            eeprom_busy = false;
            eeprom_modified = true;
            eeprom_dirty = true;
        }
        else
            eeprom_program_cycles -= cycles;
        return;
    }
}

}
