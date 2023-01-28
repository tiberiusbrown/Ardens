#include "absim.hpp"

#include <string>
#include <fstream>

#include <elfio/elfio.hpp>

namespace absim
{

static int convert_hex_char(int c)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int get_hex_byte(std::istream& f)
{
    if(f.eof()) return -1;
    int hi = convert_hex_char(f.get());
    if(hi < 0) return -1;
    if(f.eof()) return -1;
    int lo = convert_hex_char(f.get());
    if(lo < 0) return -1;
    return lo + hi * 16;
}

static char const* load_hex(atmega32u4_t& cpu, std::string const& fname)
{
    std::ifstream f(fname, std::ios::in);

    if(f.fail())
        return "Unable to open file";

    memset(&cpu.prog, 0, sizeof(cpu.prog));
    memset(&cpu.decoded_prog, 0, sizeof(cpu.decoded_prog));
    memset(&cpu.disassembled_prog, 0, sizeof(cpu.disassembled_prog));
    memset(&cpu.breakpoints, 0, sizeof(cpu.breakpoints));
    memset(&cpu.breakpoints_rd, 0, sizeof(cpu.breakpoints_rd));
    memset(&cpu.breakpoints_wr, 0, sizeof(cpu.breakpoints_wr));

    while(!f.eof())
    {
        while(f.get() != ':')
            if(f.eof())
                return "Intel HEX: unexpected EOF";
        uint8_t checksum = 0;
        int count = get_hex_byte(f);
        if(count < 0)
            return "Intel HEX bad byte count";
        checksum += (uint8_t)count;
        int addr_hi = get_hex_byte(f);
        int addr_lo = get_hex_byte(f);
        if(addr_lo < 0 || addr_hi < 0)
            return "Intel HEX: bad address";
        checksum += (uint8_t)addr_lo;
        checksum += (uint8_t)addr_hi;
        int addr = addr_lo + addr_hi * 256;
        int type = get_hex_byte(f);
        checksum += (uint8_t)type;
        if(type < 0)
            return "Intel HEX: bad type";
        if(type > 1)
            return "Intel HEX: unsupported type";
        if(type == 1)
        {
            if(count != 0)
                return "Intel HEX: non-zero byte count at end-of-file record";
            break;
        }
        if(type == 0)
        {
            for(int i = 0; i < count; ++i)
            {
                int data = get_hex_byte(f);
                if(data < 0)
                    return "Intel HEX: bad data";
                checksum += (uint8_t)data;
                if(addr + i >= cpu.prog.size())
                    return "Too many instructions!";
                if(addr + i > cpu.last_addr)
                    cpu.last_addr = addr + i;
                cpu.prog[addr + i] = (uint8_t)data;
            }
        }
        checksum = uint8_t(-checksum);
        int check = get_hex_byte(f);
        if(checksum != check)
            return "Intel HEX: bad checksum";
    }

    cpu.decode();

    return nullptr;
}

static char const* load_elf(arduboy_t& arduboy, std::string const& fname)
{
    using namespace ELFIO;
    elfio reader;

    if(!reader.load(fname))
        return "ELF: Unable to load file";

    auto& cpu = arduboy.cpu;

    memset(&cpu.prog, 0, sizeof(cpu.prog));
    memset(&cpu.decoded_prog, 0, sizeof(cpu.decoded_prog));
    memset(&cpu.disassembled_prog, 0, sizeof(cpu.disassembled_prog));
    memset(&cpu.breakpoints, 0, sizeof(cpu.breakpoints));
    memset(&cpu.breakpoints_rd, 0, sizeof(cpu.breakpoints_rd));
    memset(&cpu.breakpoints_wr, 0, sizeof(cpu.breakpoints_wr));

    for(auto const& section : reader.sections)
    {
        auto name = section->get_name();
        auto size = section->get_size();
        auto data = section->get_data();

        if(name == ".text")
        {
            if(size > cpu.PROG_SIZE_BYTES)
                return "ELF: Section .text too large";
            memcpy(&cpu.prog, data, size);
            cpu.decode();
        }
    }

    return nullptr;
}

char const* arduboy_t::load_file(char const* filename)
{
    std::string fname(filename);

    if(fname.substr(fname.size() - 4) == ".hex")
    {
        reset();
        return load_hex(cpu, fname);
    }

    if(fname.substr(fname.size() - 4) == ".elf")
    {
        reset();
        return load_elf(*this, fname);
    }

    return nullptr;
}

}
