#include "absim.hpp"

namespace absim
{

void atmega32u4_t::cycle_eeprom()
{
#define eecr data[0x3f]
#define eedr data[0x40]

    constexpr uint8_t EERIE = (1 << 3);
    constexpr uint8_t EEMPE = (1 << 2);
    constexpr uint8_t EEPE  = (1 << 1);
    constexpr uint8_t EERE  = (1 << 0);

    if(eeprom_clear_eempe_cycles != 0)
    {
        --eeprom_clear_eempe_cycles;
        if(eeprom_clear_eempe_cycles == 0)
            eecr &= ~EEMPE;
    }

    if((eecr & (EERE | EEPE | EEMPE)) == 0)
        return;

    if(just_written == 0x3f && (eecr & EEMPE) != 0)
        eeprom_clear_eempe_cycles = 4;

    if(eeprom_program_cycles != 0)
    {
        eecr |= EEPE;
        --eeprom_program_cycles;
        if(eeprom_program_cycles == 0)
        {
            eecr &= ~EEPE;
            eeprom[eeprom_write_addr] = eeprom_write_data;
        }
        return;
    }

    uint8_t eearl = data[0x41];
    uint8_t eearh = data[0x42];
    uint16_t addr = (eearl | (eearh << 8)) & 0x3ff;

    if(eecr & EEPE)
    {
        if(!(eecr & EEMPE))
        {
            eecr &= ~EEPE;
            return;
        }

        uint8_t eepm = (eecr >> 4) & 0x3;
        if(eepm == 0)
        {
            // erase and write
            eeprom_program_cycles = 16 * 3400; // 3.4 ms
            eeprom_write_addr = addr;
            eeprom_write_data = eedr;
        }
        if(eepm == 1)
        {
            // erase only
            eeprom_program_cycles = 16 * 1800; // 1.8 ms
            eeprom_write_addr = addr;
            eeprom_write_data = 0xff;
        }
        if(eepm == 2)
        {
            // write only
            eeprom_program_cycles = 16 * 1800; // 1.8 ms
            eeprom_write_addr = addr;
            eeprom_write_data = eedr & eeprom[addr];
        }
    }

    if(eecr & EERE)
    {
        eecr &= ~EERE;
        eedr = eeprom[addr];
        wakeup_cycles = 4;
        active = false;
    }
}

}
