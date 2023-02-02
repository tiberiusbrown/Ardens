#include "absim.hpp"

#include <string>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <cctype>

#include <nlohmann/json.hpp>
#include <zip_file.hpp>

#ifdef _MSC_VER
#pragma warning(push, 1)
#pragma warning(disable: 4624)
#endif

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFCompileUnit.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugAbbrev.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Object/ELFObjectFile.h>

#ifdef _MSC_VER
#pragma warning(pop) 
#endif

namespace absim
{

static std::string demangle(char const* sym)
{
    size_t size = 0;
    int status = -1;
    char* t = llvm::itaniumDemangle(sym, nullptr, &size, &status);
    if(status != 0)
        return sym;
    std::string r(t);
    std::free(t);
    return r;
}

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

static char const* load_hex(arduboy_t& a, std::istream& f)
{
    auto& cpu = a.cpu;
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
                return "HEX: unexpected EOF";
        uint8_t checksum = 0;
        int count = get_hex_byte(f);
        if(count < 0)
            return "HEX bad byte count";
        checksum += (uint8_t)count;
        int addr_hi = get_hex_byte(f);
        int addr_lo = get_hex_byte(f);
        if(addr_lo < 0 || addr_hi < 0)
            return "HEX: bad address";
        checksum += (uint8_t)addr_lo;
        checksum += (uint8_t)addr_hi;
        int addr = addr_lo + addr_hi * 256;
        int type = get_hex_byte(f);
        checksum += (uint8_t)type;
        if(type < 0)
            return "HEX: bad type";
        if(type > 1)
            return "HEX: unsupported type";
        if(type == 1)
        {
            if(count != 0)
                return "HEX: non-zero byte count at end-of-file record";
            break;
        }
        if(type == 0)
        {
            for(int i = 0; i < count; ++i)
            {
                int data = get_hex_byte(f);
                if(data < 0)
                    return "HEX: bad data";
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
            return "HEX: bad checksum";
    }

    cpu.decode();

    return nullptr;
}

static char const* load_hex(arduboy_t& a, std::string const& fname)
{
    std::ifstream f(fname, std::ios::in);

    if(f.fail())
        return "HEX: Unable to open file";

    return load_hex(a, f);
}

struct Elf32_Sym
{
    uint32_t  st_name;
    uint32_t  st_value;
    uint32_t  st_size;
    uint8_t   st_info;
    uint8_t   st_other;
    uint16_t  st_shndx;
};

static void try_insert_symbol(elf_data_t::map_type& m, elf_data_symbol_t& sym)
{
    auto it = m.find(sym.addr);
    if(it == m.end())
    {
        m[sym.addr] = std::move(sym);
        return;
    }
    // if new is WEAK don't replace old with it
    if(sym.weak)
        return;
    auto& oldsym = it->second;
    // if new is NOTYPE and old is not NOTYPE don't replace
    if(sym.notype && !oldsym.notype)
        return;
    // if old is GLOBAL don't replace it
    if(oldsym.global)
        return;
    m[sym.addr] = std::move(sym);
}

static void add_file_to_elf(elf_data_t& elf,
    uint16_t addr, std::string const& filename, int line)
{
    if(elf.source_lines.count(addr) != 0) return;
    if(elf.source_file_names.count(filename) == 0)
    {
        // load file contents
        std::ifstream f(filename);
        if(f.fail()) return;
        int i = (int)elf.source_files.size();
        elf.source_files.resize(i + 1);
        auto& sf = elf.source_files.back();
        sf.filename = filename;
        elf.source_file_names[filename] = i;
        std::string linestr;
        while(std::getline(f, linestr))
        {
            //trim(linestr);
            sf.lines.push_back(linestr);
        }
        elf.source_lines[addr] = { i, line };
    }
    else
        elf.source_lines[addr] = { elf.source_file_names[filename], line };
}

template<class T> T defval(llvm::Expected<T> e)
{
    return e ? e.get() : T{};
}

static void load_elf_debug(
    arduboy_t& a, absim::elf_data_t& elf,
    llvm::object::ELFObjectFileBase* obj,
    llvm::DWARFContext* dwarf_ctx)
{
    using namespace llvm;

    auto& cpu = a.cpu;

    // get frame unwind info
    if(auto frame_or_err = dwarf_ctx->getDebugFrame())
    {
        auto frame = frame_or_err.get();
        for(auto const& e : *frame)
        {
            auto* fde = dyn_cast<dwarf::FDE>(&e);
            if(!fde) continue;
            auto pc_start = fde->getInitialLocation();
            auto pc_size = fde->getAddressRange();
            auto rows_or_err = dwarf::UnwindTable::create(fde);
            if(!rows_or_err) continue;

            absim::elf_data_t::frame_info_t fi;
            fi.addr_lo = uint16_t(pc_start);
            fi.addr_hi = uint16_t(pc_start + pc_size);

            for(auto const& row : rows_or_err.get())
            {
                if(!row.hasAddress()) continue;
                auto addr = row.getAddress();
                auto const& cfa = row.getCFAValue();
                auto reg = cfa.getRegister();
                auto off = cfa.getOffset();
                auto reglocs = row.getRegisterLocations();
                absim::elf_data_t::frame_info_t::unwind_t u;
                u.addr = (uint16_t)addr;
                u.cfa_reg = (uint8_t)reg;
                u.cfa_offset = (int16_t)off;
                for(uint32_t i = 0; i < 32; ++i)
                {
                    auto loc = reglocs.getRegisterLocation(i);
                    u.reg_offsets[i] = loc ? (int16_t)loc.getValue().getOffset() : INT16_MAX;
                }
                auto loc = reglocs.getRegisterLocation(36);
                if(!loc) continue;
                u.ra_offset = (int16_t)loc.getValue().getOffset();
                fi.unwinds.push_back(u);
            }
            if(fi.unwinds.empty())
                continue;
            std::sort(fi.unwinds.begin(), fi.unwinds.end(),
                [](auto const& a, auto const& b) { return a.addr < b.addr; });
            if(fi.unwinds[0].addr != fi.addr_lo)
                continue;
            elf.frames.push_back(fi);
        }
    }
    std::sort(elf.frames.begin(), elf.frames.end(),
        [](auto const& a, auto const& b) { return a.addr_lo < b.addr_lo; });

    // find source lines for each address
    for(size_t a = 0; a < cpu.last_addr; a += 2)
    {
        auto* cu = dwarf_ctx->getCompileUnitForAddress(a);
        if(!cu) continue;
        auto* tab = dwarf_ctx->getLineTableForUnit(cu);
        if(!tab) continue;
        auto& rows = tab->Rows;
        auto r = llvm::DWARFDebugLine::Row();
        r.Address = { a };
        auto it = std::lower_bound(rows.begin(), rows.end(), r,
            [](auto const& a, auto const& b) { return a.Address.Address < b.Address.Address; });
        if(it == rows.end() || it->Address.Address != a)
            continue;
        if(!it->IsStmt) __debugbreak();
        std::string source_file;
        if(!tab->getFileNameByIndex(it->File, cu->getCompilationDir(),
            llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
            source_file))
            continue;
        add_file_to_elf(elf, (uint16_t)a, source_file, (int)it->Line - 1);
    }

    // create intermixed asm with source/symbols
    {
        auto& v = elf.asm_with_source;
        for(uint16_t i = 0; i < cpu.num_instrs; ++i)
        {
            uint16_t addr = cpu.disassembled_prog[i].addr;
            disassembled_instr_t instr;
            instr.addr = addr;
            auto its = elf.text_symbols.find(addr);
            if(its != elf.text_symbols.end())
            {
                instr.type = disassembled_instr_t::SYMBOL;
                v.push_back(instr);
            }
            auto it = elf.source_lines.find(addr);
            if(it != elf.source_lines.end())
            {
                instr.type = disassembled_instr_t::SOURCE;
                v.push_back(instr);
            }
            v.push_back(cpu.disassembled_prog[i]);
        }
    }
}

static char const* load_elf(arduboy_t& a, std::string const& fname)
{
    using namespace llvm;

    std::ifstream f(fname, std::ios::binary);
    std::vector<char> fdata(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());

    MemoryBufferRef mbuf(
        { fdata.data(), fdata.size() },
        { fname.c_str(), fname.size() });

    auto bin_or_err = object::createBinary(mbuf);
    if(!bin_or_err) return "ELF: could not load file";

    auto* obj = dyn_cast<object::ELFObjectFileBase>(bin_or_err->get());
    if(!obj)
        return "ELF: could not load object file";

    if(obj->getEMachine() != 0x0053)
        return "ELF: machine not EM_AVR";

    auto& cpu = a.cpu;

    a.elf.reset();
    memset(&cpu.prog, 0, sizeof(cpu.prog));
    memset(&cpu.decoded_prog, 0, sizeof(cpu.decoded_prog));
    memset(&cpu.disassembled_prog, 0, sizeof(cpu.disassembled_prog));
    memset(&cpu.breakpoints, 0, sizeof(cpu.breakpoints));
    memset(&cpu.breakpoints_rd, 0, sizeof(cpu.breakpoints_rd));
    memset(&cpu.breakpoints_wr, 0, sizeof(cpu.breakpoints_wr));
    bool found_text = false;

    auto elf_ptr = std::make_unique<elf_data_t>();
    auto& elf = *elf_ptr;

    constexpr uint32_t ram_offset = 0x800000;

    object::SectionRef sec_data;
    object::SectionRef sec_bss;
    object::SectionRef sec_text;

    for(auto const& section : obj->sections())
    {
        auto name = defval(section.getName());
        auto data = defval(section.getContents());
        auto addr = section.getAddress();
        auto size = data.size();
        if(!name.data() || !data.data()) continue;

        if(name == ".text")
        {
            sec_text = section;
            if(size > cpu.PROG_SIZE_BYTES)
                return "ELF: Section .text too large";
            memcpy(&cpu.prog, data.data(), size);
            cpu.last_addr = (uint16_t)size;
            found_text = true;
        }

        if(name == ".data")
        {
            sec_data = section;
            elf.data_begin = uint16_t(addr - ram_offset);
            elf.data_end = uint16_t(elf.data_begin + size);
        }

        if(name == ".bss")
        {
            sec_bss = section;
            elf.bss_begin = uint16_t(addr - ram_offset);
            elf.bss_end = uint16_t(elf.data_begin + size);
        }

    }

    if(!found_text)
        return "ELF: No .text section";

    cpu.decode();

    for(auto const& symbol : obj->symbols())
    {
        auto name = defval(symbol.getName());
        auto addr = defval(symbol.getAddress());
        auto type = defval(symbol.getType());
        auto flags = defval(symbol.getFlags());
        auto sec_or_err = symbol.getSection();
        if(!sec_or_err) continue;
        auto sec = sec_or_err.get();
        auto size = symbol.getSize();
        if(!name.data())
            continue;
        if(name == "FONT_IMG") __debugbreak();
        if(*sec != sec_text && *sec != sec_data && *sec != sec_bss)
            continue;

        switch(type)
        {
        case object::SymbolRef::Type::ST_Unknown:
        case object::SymbolRef::Type::ST_Data:
        case object::SymbolRef::Type::ST_Function:
            break;
        default:
            continue;
        }

        bool is_text = (sec == sec_text);
        if(!is_text)
        {
            addr -= ram_offset;
            if(addr >= 2560) continue;
        }

        if(name == "__eeprom_end") __debugbreak();

        elf_data_symbol_t sym;
        sym.name = demangle(name.data());
        sym.addr = (uint16_t)addr;
        sym.size = (uint16_t)size;
        sym.global = (flags & object::SymbolRef::Flags::SF_Global) != 0;
        sym.weak   = (flags & object::SymbolRef::Flags::SF_Weak) != 0;
        sym.notype = (type == object::SymbolRef::Type::ST_Unknown);
        sym.object = (type == object::SymbolRef::Type::ST_Data);
        try_insert_symbol(is_text ? elf.text_symbols : elf.data_symbols, sym);
    }

    // note object text symbols in disassembly
    for(auto const& kv : elf.text_symbols)
    {
        auto const& sym = kv.second;
        if(!sym.object) continue;
        auto addr = sym.addr;
        auto addr_end = addr + sym.size;
        while(addr < addr_end)
        {
            auto i = cpu.addr_to_disassembled_index(addr);
            cpu.disassembled_prog[i].type = disassembled_instr_t::OBJECT;
            addr += 2;
        }
    }

    // create intermixed asm with source/symbols
    {
        auto& v = elf.asm_with_source;
        for(uint16_t i = 0; i < cpu.num_instrs; ++i)
        {
            uint16_t addr = cpu.disassembled_prog[i].addr;
            disassembled_instr_t instr;
            instr.addr = addr;
            auto its = elf.text_symbols.find(addr);
            if(its != elf.text_symbols.end())
            {
                instr.type = disassembled_instr_t::SYMBOL;
                v.push_back(instr);
            }
            auto it = elf.source_lines.find(addr);
            if(it != elf.source_lines.end())
            {
                instr.type = disassembled_instr_t::SOURCE;
                v.push_back(instr);
            }
            v.push_back(cpu.disassembled_prog[i]);
        }
    }

    // load debug info
    auto dwarf_ctx = DWARFContext::create(*obj);
    load_elf_debug(a, elf, obj, dwarf_ctx.get());

    a.elf.swap(elf_ptr);

    return nullptr;
}

static char const* load_bin(arduboy_t& a, std::istream& f)
{
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
    size_t n = data.size();
    n = (n + 255) & 0x1ffff00;
    data.resize(n);
    if(n > a.fx.data.size())
        return "BIN: FX data too large";

    a.fx.erase_all_data();
    if(n != 0)
        memcpy(&a.fx.data[a.fx.data.size() - n], &data[0], n);

    return nullptr;
}

static char const* load_bin(arduboy_t& a, std::string const& fname)
{
    std::ifstream f(fname, std::ios::in | std::ios::binary);

    if(f.fail())
        return "BIN: Unable to open file";

    return load_bin(a, f);
}

static char const* load_arduboy(arduboy_t& a, std::string const& fname)
{
    miniz_cpp::zip_file z(fname);
    if(!z.has_file("info.json"))
        return "ARDUBOY: No info.json file";
    std::string info = z.read("info.json");

    auto j = nlohmann::json::parse(info);
    if(!j.contains("binaries"))
        return "ARDUBOY: info.json missing 'binaries'";
    auto const& bins = j["binaries"];
    if(!bins.is_array())
        return "ARDUBOY: 'binaries' not array";
    auto const& bin = bins[0];
    if(!bin.contains("filename"))
        return "ARDUBOY: primary binary missing 'filename'";
    auto const hexfile = bin["filename"];
    if(!hexfile.is_string())
        return "ARDUBOY: primary binary filename not string type";

    if(!z.has_file(hexfile))
        return "ARDUBOY: missing hex file indicated in info.json";

    {
        std::string hexdata = z.read(hexfile);
        std::stringstream ss(hexdata);
        char const* err = load_hex(a, ss);
        if(err) return err;
    }

    if(bin.contains("flashdata"))
    {
        auto const& binfile = bin["flashdata"];
        if(!binfile.is_string())
            return "ARDUBOY: FX data filename not string type";
        if(!z.has_file(binfile))
            return "ARDUBOY: missing FX data file indicated in info.json";

        std::string bindata = z.read(binfile);
        std::stringstream ss(bindata);
        char const* err = load_bin(a, ss);
        if(err) return err;
    }

    // TODO: bin file

    return nullptr;
}

static bool ends_with(std::string const& str, std::string const& end)
{
    if(str.size() < end.size()) return false;
    return str.substr(str.size() - end.size()) == end;
}

char const* arduboy_t::load_file(char const* filename)
{
    std::string fname(filename);

    if(ends_with(fname, ".hex"))
    {
        reset();
        return load_hex(*this, fname);
    }

    if(ends_with(fname, ".bin"))
    {
        return load_bin(*this, fname);
    }

    if(ends_with(fname, ".elf"))
    {
        reset();
        return load_elf(*this, fname);
    }

    if(ends_with(fname, ".arduboy"))
    {
        reset();
        return load_arduboy(*this, fname);
    }

    return nullptr;
}

}
