#include "absim.hpp"

namespace absim
{

void atmega32u4_t::eeprom_handle_st_eecr(
    atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    assert(ptr == reg::addr::EECR);

    constexpr uint8_t EEMPE = reg::bit::EECR::EEMPE;
    constexpr uint8_t EEPE = reg::bit::EECR::EEPE;
    constexpr uint8_t EERE = reg::bit::EECR::EERE;

    auto& eecr = cpu.data[reg::addr::EECR];
    auto& eedr = cpu.data[reg::addr::EEDR];

    if(x & EEMPE)
    {
        cpu.eeprom_clear_eempe_cycles = 5;
        cpu.eeprom_busy = true;
        cpu.peripheral_queue.schedule(
            cpu.cycle_count + cpu.eeprom_clear_eempe_cycles, PQ_EEPROM);
    }

    uint16_t eearl = cpu.data[reg::addr::EEARL];
    uint16_t eearh = cpu.data[reg::addr::EEARH];
    uint16_t addr = (eearl | (eearh << 8));
    if(addr >= 0x400)
        cpu.autobreak(AB_OOB_EEPROM);
    addr &= 0x3ff;

    if(cpu.eeprom_program_cycles == 0 && (x & EEPE))
    {
        if(!(eecr & EEMPE))
            x &= ~EEPE;
        else
        {
            uint8_t eepm = (x >> 4) & 0x3;

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
            if(cpu.eeprom_program_cycles != 0)
            {
                cpu.peripheral_queue.schedule(
                    cpu.cycle_count + cpu.eeprom_program_cycles, PQ_EEPROM);
            }
        }
    }

    if(x & EERE)
    {
        x &= ~EERE;
        eedr = cpu.eeprom[addr];
        cpu.wakeup_cycles = 4;
        cpu.active = false;
        cpu.eeprom_busy = false;
    }

    eecr = x;
}

ARDENS_FORCEINLINE void atmega32u4_t::update_eeprom()
{
    if(!eeprom_busy)
        return;

    uint32_t cycles = uint32_t(cycle_count - eeprom_prev_cycle);
    eeprom_prev_cycle = cycle_count;

    auto& eecr = data[reg::addr::EECR];

    constexpr uint8_t EEMPE = reg::bit::EECR::EEMPE;
    constexpr uint8_t EEPE  = reg::bit::EECR::EEPE;

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
        {
            eeprom_clear_eempe_cycles -= cycles;
            peripheral_queue.schedule(
                cycle_count + eeprom_clear_eempe_cycles, PQ_EEPROM);
        }
    }

    if(eeprom_program_cycles != 0)
    {
        eecr |= EEPE;

        if(eeprom_program_cycles <= cycles)
        {
            eeprom_program_cycles = 0;
            eecr &= ~EEPE;
            eeprom_write_addr &= 1023;
            eeprom[eeprom_write_addr] = eeprom_write_data;
            eeprom_busy = false;
            eeprom_modified = true;
            eeprom_dirty = true;
            eeprom_modified_bytes.set(eeprom_write_addr);
        }
        else
        {
            eeprom_program_cycles -= cycles;
            peripheral_queue.schedule(
                cycle_count + eeprom_program_cycles, PQ_EEPROM);
        }
    }
}

}
