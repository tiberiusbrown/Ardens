#include "absim.hpp"

namespace absim
{

void w25q128_t::erase_all_data()
{
    memset(&data, 0xff, sizeof(data));
    memcpy(&data, "ARDUBOY", 7);
    sectors_modified.reset();
}

void w25q128_t::reset()
{
    enabled = false;
    woken_up = false;
    write_enabled = false;
    reading_status = false;
    processing_command = false;
    command.clear();

    reading = 0;
    programming = 0;
    erasing_sector = 0;
    releasing = 0;

    busy_ps_rem = 0;
    current_addr = 0;

    sectors_dirty = false;
}

ARDENS_FORCEINLINE void w25q128_t::set_enabled(bool e)
{
    if(enabled != e)
    {
        enabled = e;
        if(!e)
        {
            reading_status = false;
            reading = false;
            programming = false;
            processing_command = false;
            releasing = 0;
            if(busy_ps_rem == 0)
                command.clear();
        }
    }
}

ARDENS_FORCEINLINE void w25q128_t::advance(uint64_t ps)
{
    if(ps >= busy_ps_rem)
    {
        if(!enabled)
            command.clear();
        busy_ps_rem = 0;
    }
    else
        busy_ps_rem -= ps;
}

ARDENS_FORCEINLINE void w25q128_t::track_page()
{
    current_addr &= 0xffffff;
    uint32_t page = current_addr / 256;
    min_page = std::min(min_page, page);
    max_page = std::max(max_page, page);
}

ARDENS_FORCEINLINE uint8_t w25q128_t::spi_transceive(uint8_t byte)
{
    if(!enabled) return 0;

    uint8_t data_to_send = 0;

    if(reading)
    {
        if(reading <= 3)
        {
            current_addr = (current_addr << 8) | byte;
            ++reading;
        }
        if(reading >= 4)
        {
            //track_page();
            data_to_send = data[current_addr];
            ++current_addr;
            current_addr &= 0xffffff;
        }
    }
    else if(programming)
    {
        if(programming == 4)
        {
            track_page();
            uint32_t page = current_addr & 0xffff00;
            sectors_modified.set(current_addr >> 12);
            sectors_dirty = true;
            data[current_addr] &= byte;
            ++current_addr;
            current_addr = page | (current_addr & 0xff);
            data_to_send = 0;
            busy_ps_rem = 700ull * 1000 * 1000; // 0.7 ms
        }
        else
        {
            current_addr = (current_addr << 8) | byte;
            ++programming;
        }
    }
    else if(reading_status)
    {
        if(busy_ps_rem != 0)
            data_to_send = 0x1;
    }
    else if(erasing_sector)
    {
        if(erasing_sector <= 3)
        {
            current_addr = (current_addr << 8) | byte;
            ++erasing_sector;
        }
        if(erasing_sector == 4)
        {
            current_addr &= 0xfff000;
            track_page();
            memset(&data[current_addr], 0xff, 0x1000);
            sectors_modified.set(current_addr >> 12);
            sectors_dirty = true;
            busy_ps_rem = 100ull * 1000 * 1000 * 1000; // 100 ms
            erasing_sector = 0;
        }
    }
    else if(releasing)
    {
        if(releasing <= 3)
            ++releasing;
        if(releasing == 4)
            data_to_send = 0x17;
    }
    else if(!woken_up)
    {
        if(byte == 0xab)
        {
            processing_command = true;
            command = "Release Power-down";
            woken_up = true;
            releasing = 1;
        }
    }
    else if(!processing_command)
    {
        processing_command = true;
        switch(byte)
        {
        case 0x02: // program page
            if(!write_enabled || busy_ps_rem != 0) break;
            command = "Page Program";
            programming = 1;
            current_addr = 0;
            break;
        case 0x03: // read data
            if(busy_ps_rem != 0) break;
            command = "Read Data";
            reading = 1;
            current_addr = 0;
            break;
        case 0x04: // write disable
            if(busy_ps_rem != 0) break;
            command = "Write disable";
            write_enabled = false;
            break;
        case 0x05: // read status register 1
            command = "Read Status Register-1";
            reading_status = true;
            if(busy_ps_rem != 0)
                data_to_send = 0x1;
            break;
        case 0x06: // write enable
            if(busy_ps_rem != 0) break;
            command = "Write Disable";
            write_enabled = true;
            break;
        case 0x20: // sector erase
            if(!write_enabled || busy_ps_rem != 0) break;
            command = "Sector Erase";
            erasing_sector = 1;
            current_addr = 0;
            break;
        case 0xb9:
            woken_up = false;
            break;
        default:
            command = "<unknown>";
            break;
        }
    }

    return data_to_send;
}

}
