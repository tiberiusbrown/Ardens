#ifndef _SILENCE_CXX17_STRSTREAM_DEPRECATION_WARNING
#define _SILENCE_CXX17_STRSTREAM_DEPRECATION_WARNING
#endif

#include "absim.hpp"

#include <string>
#include <fstream>
#include <sstream>
#include <iterator>
#include <vector>
#include <algorithm>
#include <cctype>

#include <strstream>

#ifndef ARDENS_NO_ARDUBOY_FILE
#include <yyjson/yyjson.h>
#include <miniz.h>
#include <miniz_zip.h>
#endif

#ifdef ARDENS_LLVM
#ifdef _MSC_VER
#pragma warning(push, 1)
#pragma warning(disable: 4244)
#pragma warning(disable: 4624)
#endif
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFCompileUnit.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugAbbrev.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Object/ELFObjectFile.h>
#include "dwarf.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif

namespace absim
{

static bool icompare(std::string const& a, std::string const& b)
{
    std::string ta = a, tb = b;
    for(char& c : ta) if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
    for(char& c : tb) if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return ta < tb;
}

size_t elf_data_t::addr_to_disassembled_index(uint16_t addr)
{
    disassembled_instr_t temp;
    temp.addr = addr;
    auto it = std::lower_bound(
        asm_with_source.begin(),
        asm_with_source.end(),
        temp,
        [](auto const& a, auto const& b) { return a.addr < b.addr; }
    );

    auto index = std::distance(asm_with_source.begin(), it);

    return (size_t)index;
}

elf_data_t::~elf_data_t() {}

#ifdef ARDENS_LLVM
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
#endif

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

static void find_stack_check_data(atmega32u4_t& cpu, uint16_t n)
{
    if(n + 11 >= cpu.decoded_prog.size()) return;
    uint8_t first_reg;
    uint32_t val;
    auto const* i = &cpu.decoded_prog[n];
    if(i->func != INSTR_LDI) return;
    first_reg = i->dst;
    val = (uint32_t(i->src) << 8);
    i = &cpu.decoded_prog[n + 1];
    if(i->func != INSTR_LDI || i->dst != 26) return;
    i = &cpu.decoded_prog[n + 2];
    if(i->func != INSTR_LDI || i->dst != 27) return;
    i = &cpu.decoded_prog[n + 3];
    if(i->func != INSTR_LDI || i->dst != 30) return;
    i = &cpu.decoded_prog[n + 4];
    if(i->func != INSTR_LDI || i->dst != 31) return;
    i = &cpu.decoded_prog[n + 5];
    if(i->func != INSTR_RJMP || i->word != 2) return;
    i = &cpu.decoded_prog[n + 6];
    if(i->func != INSTR_LPM || i->word != 1 || i->dst != 0) return;
    i = &cpu.decoded_prog[n + 7];
    if(i->func != INSTR_LD_ST || i->dst <= 10 || i->word == 0 || i->src != 0) return;
    i = &cpu.decoded_prog[n + 8];
    if(i->func != INSTR_CPI || i->dst != 26) return;
    val |= i->src;
    i = &cpu.decoded_prog[n + 9];
    if(i->func != INSTR_CPC || i->dst != 27 || i->src != first_reg) return;
    i = &cpu.decoded_prog[n + 10];
    if(i->func != INSTR_BRBC || i->src != 1) return;
    cpu.stack_check = std::max(cpu.stack_check, val);
}

static void find_stack_check_bss(atmega32u4_t& cpu, uint16_t n)
{
    if(n + 8 >= cpu.decoded_prog.size()) return;
    uint8_t first_reg;
    uint32_t val;
    auto const* i = &cpu.decoded_prog[n];
    if(i->func != INSTR_LDI) return;
    first_reg = i->dst;
    val = (uint32_t(i->src) << 8);
    i = &cpu.decoded_prog[n + 1];
    if(i->func != INSTR_LDI || i->dst != 26) return;
    i = &cpu.decoded_prog[n + 2];
    if(i->func != INSTR_LDI || i->dst != 27) return;
    i = &cpu.decoded_prog[n + 3];
    if(i->func != INSTR_RJMP || i->word != 1) return;
    i = &cpu.decoded_prog[n + 4];
    if(i->func != INSTR_LD_ST || i->dst <= 10 || i->word == 0 || i->src != 1) return;
    i = &cpu.decoded_prog[n + 5];
    if(i->func != INSTR_CPI || i->dst != 26) return;
    val |= i->src;
    i = &cpu.decoded_prog[n + 6];
    if(i->func != INSTR_CPC || i->dst != 27 || i->src != first_reg) return;
    i = &cpu.decoded_prog[n + 7];
    if(i->func != INSTR_BRBC || i->src != 1) return;
    cpu.stack_check = std::max(cpu.stack_check, val);
}

// try to extract stack limit from disassembly
static void find_stack_check(atmega32u4_t& cpu)
{
    cpu.stack_check = 0x100;

    uint16_t reset_instr = 0;

    {
        auto const& i = cpu.decoded_prog[0];
        if(i.func == INSTR_JMP)
            reset_instr = i.word;
        else if(i.func == INSTR_RJMP)
            reset_instr = i.word - 1;
        else
            return;
    }

    for(uint16_t n = reset_instr; n < reset_instr + 32; ++n)
    {
        if(n >= cpu.decoded_prog.size())
            return;
        find_stack_check_data(cpu, n);
        find_stack_check_bss(cpu, n);
    }
}

static std::string load_hex(arduboy_t& a, std::istream& f, bool bootloader = false)
{
    auto& cpu = a.cpu;
    if(!bootloader)
    {
        memset(&cpu.prog, 0, 29 * 1024);
        memset(&cpu.decoded_prog, 0, sizeof(cpu.decoded_prog));
        memset(&cpu.disassembled_prog, 0, sizeof(cpu.disassembled_prog));
        memset(&cpu.eeprom, 0xff, sizeof(cpu.eeprom));
        cpu.eeprom_modified = false;

        a.breakpoints.reset();
        a.breakpoints_rd.reset();
        a.breakpoints_wr.reset();
    }

    uint32_t addr_upper = 0;
    uint32_t num_records = 0;

    while(!f.eof())
    {
        while(!f.eof() && f.get() != ':')
            ;
        if(f.eof())
            break;
        uint8_t checksum = 0;
        int count = get_hex_byte(f);
        ++num_records;
        if(count < 0)
            return "HEX bad byte count";
        checksum += (uint8_t)count;
        uint32_t addr_hi = (uint32_t)get_hex_byte(f);
        uint32_t addr_lo = (uint32_t)get_hex_byte(f);
        if(addr_lo < 0 || addr_hi < 0)
            return "HEX: bad address";
        checksum += (uint8_t)addr_lo;
        checksum += (uint8_t)addr_hi;
        uint32_t addr = addr_lo + addr_hi * 256;
        addr += (addr_upper << 16);
        int type = get_hex_byte(f);
        checksum += (uint8_t)type;
        if(type < 0 || type > 5)
            return "HEX: bad type";
        if(type == 0)
        {
            for(int i = 0; i < count; ++i)
            {
                int data = get_hex_byte(f);
                if(data < 0)
                    return "HEX: bad data";
                checksum += (uint8_t)data;
                if(addr >= 0x800000)
                    continue;
                if(addr + i >= cpu.prog.size())
                    return "Too many instructions!";
                if(!bootloader && addr + i > cpu.last_addr)
                    cpu.last_addr = addr + i;
                cpu.prog[addr + i] = (uint8_t)data;
            }
        }
        else if(type == 1)
        {
            if(count != 0)
                return "HEX: non-zero byte count at end-of-file record";
            break;
        }
        else if(type == 4)
        {
            addr_upper = 0;
            for(int i = 0; i < count; ++i)
            {
                int data = get_hex_byte(f);
                if(data < 0)
                    return "HEX: bad data";
                checksum += (uint8_t)data;
                addr_upper <<= 8;
                addr_upper |= (uint32_t)data;
            }
        }
        else if(type == 3)
        {
            // ignore
            for(int i = 0; i < count; ++i)
                checksum += (uint8_t)get_hex_byte(f);
        }
        else
        {
            return "HEX: unsupported type";
        }
        checksum = uint8_t(-checksum);
        int check = get_hex_byte(f);
        if(checksum != check)
            return "HEX: bad checksum";
    }

    if(num_records == 0)
        return "HEX: no records found";

    if(!bootloader)
        cpu.decode();

    if(!bootloader)
        find_stack_check(cpu);

    return "";
}

static std::string load_hex(arduboy_t& a, std::string const& fname)
{
    std::ifstream f(fname, std::ios::in);

    if(f.fail())
        return "HEX: Unable to open file";

    return load_hex(a, f);
}

#ifdef ARDENS_LLVM
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

static void load_elf_debug_recurse_globals(
    arduboy_t& a,
    absim::elf_data_t& elf,
    std::string prefix,
    llvm::DWARFContext* dwarf_ctx,
    llvm::DWARFUnit const* cu,
    llvm::DWARFDie die)
{
    if(die.getTag() == llvm::dwarf::DW_TAG_namespace)
    {
        for(auto child : die)
        {
            load_elf_debug_recurse_globals(
                a, elf,
                prefix + dwarf_name(die) + "::",
                dwarf_ctx, cu, child);
        }
        return;
    }
    if(die.getTag() == llvm::dwarf::DW_TAG_subprogram)
    {
        for(auto child : die)
        {
            load_elf_debug_recurse_globals(
                a, elf,
                prefix + dwarf_function_args_string(die) + "::",
                dwarf_ctx, cu, child);
        }
        return;
    }
    if(die.getTag() != llvm::dwarf::DW_TAG_variable) return;
    elf_data_t::global_t g{};
    g.addr = 0xffffffff;
    g.type = 0xffffffff;
    char const* name = die.getShortName();
    if(!name) return;
    g.cu_offset = cu->getOffset();
    for(auto const& a : die.attributes())
    {
        if(a.Attr == llvm::dwarf::DW_AT_location)
        {
            auto opt = a.Value.getAsBlock();
            if(!opt.has_value())
                continue;
            auto expr = *opt;
            llvm::DataExtractor data(llvm::StringRef(
                (char const*)expr.data(), expr.size()),
                dwarf_ctx->isLittleEndian(), 0);
            llvm::DWARFExpression e(
                data, cu->getAddressByteSize(), cu->getFormParams().Format);
            for(auto const& op : e)
            {
                if(op.getCode() == llvm::dwarf::DW_OP_addr)
                    g.addr = (uint32_t)op.getRawOperand(0);
            }
        }
        if(a.Attr == llvm::dwarf::DW_AT_type)
            g.type = (uint32_t)a.Value.getAsReference().value_or(0xffffffff);
        if(a.Attr == llvm::dwarf::DW_AT_decl_file)
            g.file = (int)a.Value.getAsUnsignedConstant().value_or(0);
        if(a.Attr == llvm::dwarf::DW_AT_decl_line)
            g.line = (int)a.Value.getAsUnsignedConstant().value_or(0);
    }
    std::string sname = prefix + name;
    if(g.addr == 0xffffffff)
    {
        // try to identify with data symbol
        for(auto const& kv : elf.data_symbols)
        {
            if(kv.second.name == sname)
            {
                g.addr = 0x800000 + kv.second.addr;
                break;
            }
        }
    }
    if(g.addr == 0xffffffff) return;
    if(g.type == 0xffffffff) return;
    if(g.addr >= 0x800000)
        g.addr -= 0x800000;
    else
        g.text = true;
    if(g.text && g.addr >= a.cpu.prog.size()) return;
    if(!g.text && g.addr >= a.cpu.data.size()) return;
    elf.globals[sname] = g;
}

static void load_elf_debug(
    arduboy_t& a, absim::elf_data_t& elf,
    llvm::object::ELFObjectFileBase* obj,
    llvm::DWARFContext* dwarf_ctx)
{
    using namespace llvm;

    auto& cpu = a.cpu;

#if 0
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
#endif

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
        if(!it->IsStmt) continue;
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
            auto const& di = cpu.disassembled_prog[i];
            uint16_t addr = di.addr;
            disassembled_instr_t instr;

            if(di.type == disassembled_instr_t::OBJECT)
            {
                instr.obj_bytes = 1;
                for(int j = 0; j < di.obj_bytes; ++j)
                {
                    auto its = elf.text_symbols.find(addr + j);
                    instr.addr = addr + j;
                    if(its != elf.text_symbols.end())
                    {
                        instr.type = disassembled_instr_t::SYMBOL;
                        v.push_back(instr);
                    }
                    instr.type = disassembled_instr_t::OBJECT;
                    v.push_back(instr);
                }
                continue;
            }

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

    // merge adjacent object types
    constexpr int MAX_OBJ_BYTES = 8;
    for(size_t m, n = 0; n < elf.asm_with_source.size(); ++n)
    {
        auto& i = elf.asm_with_source[n];
        if(i.type != disassembled_instr_t::OBJECT) continue;
        for(m = n + 1; m < elf.asm_with_source.size(); ++m)
        {
            auto& j = elf.asm_with_source[m];
            if(j.type != disassembled_instr_t::OBJECT) break;
            i.obj_bytes += j.obj_bytes;
            j.obj_bytes = 0;
            j.addr = i.addr + i.obj_bytes;
            if(i.obj_bytes > MAX_OBJ_BYTES)
            {
                j.obj_bytes = i.obj_bytes - MAX_OBJ_BYTES;
                i.obj_bytes = MAX_OBJ_BYTES;
                j.addr = i.addr + i.obj_bytes;
                break;
            }
        }
        auto bytes = i.obj_bytes;
        {
            elf.asm_with_source.erase(
                elf.asm_with_source.begin() + (n + 1),
                elf.asm_with_source.begin() + m);
        }
        if(bytes == 0)
        {
            elf.asm_with_source.erase(elf.asm_with_source.begin() + n);
            --n;
        }
    }

    // track globals
    for(auto const& cu : dwarf_ctx->compile_units())
    {
        auto const& u = cu->getUnitDIE();
        for(auto const& v : u.children())
        {
            load_elf_debug_recurse_globals(a, elf, "", dwarf_ctx, cu.get(), v);
        }
    }
}

static std::string load_elf(arduboy_t& a, std::istream& f, std::string const& fname)
{
    using namespace llvm;

    a.elf.reset();
    auto& cpu = a.cpu;
    memset(&cpu.prog, 0, sizeof(cpu.prog));
    memset(&cpu.decoded_prog, 0, sizeof(cpu.decoded_prog));
    memset(&cpu.disassembled_prog, 0, sizeof(cpu.disassembled_prog));
    memset(&a.breakpoints, 0, sizeof(a.breakpoints));
    memset(&a.breakpoints_rd, 0, sizeof(a.breakpoints_rd));
    memset(&a.breakpoints_wr, 0, sizeof(a.breakpoints_wr));
    bool found_text = false;

    auto elf_ptr = std::make_unique<elf_data_t>();
    auto& elf = *elf_ptr;

    elf.fdata = std::vector<char>(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());

    MemoryBufferRef mbuf(
        { elf.fdata.data(), elf.fdata.size() },
        { fname.c_str(), fname.size() });

    switch(identify_magic(mbuf.getBuffer()))
    {
    case file_magic::elf:
    case file_magic::elf_relocatable:
    case file_magic::elf_executable:
    case file_magic::elf_shared_object:
    case file_magic::elf_core:
        break;
    default:
        return "ELF: non-ELF magic";
    }
    auto bin_or_err = llvm::object::ObjectFile::createELFObjectFile(mbuf);
    if(!bin_or_err) return "ELF: could not load file";

    auto* obj = dyn_cast<object::ELFObjectFileBase>(bin_or_err->get());

    if(!obj)
        return "ELF: could not load object file";

    if(obj->getEMachine() != 0x0053)
        return "ELF: machine not EM_AVR";

    constexpr uint32_t ram_offset = 0x800000;

    object::SectionRef sec_data;
    object::SectionRef sec_bss;
    object::SectionRef sec_text;

    for(auto const& section : obj->sections())
    {
        auto name = defval(section.getName());
        auto data = defval(section.getContents());
        auto addr = section.getAddress();
        auto size = section.getSize();
        if(!name.data() || !data.data()) continue;

        if(name == ".text")
        {
            sec_text = section;
            if(size > cpu.prog.size())
                return "ELF: Section .text too large";
            memcpy(cpu.prog.data(), data.data(), array_bytes(data));
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
            elf.bss_end = uint16_t(elf.bss_begin + size);
        }

    }

    cpu.stack_check = 0x100;
    if(sec_bss.getObject())
        cpu.stack_check = std::max<uint32_t>(cpu.stack_check, elf.bss_end);
    if(sec_data.getObject())
        cpu.stack_check = std::max<uint32_t>(cpu.stack_check, elf.data_end);

    if(!found_text)
        return "ELF: No .text section";

    // copy data
    if(sec_data.getObject())
    {
        auto size = sec_data.getSize();
        auto data = defval(sec_data.getContents());
        if(cpu.last_addr + size > cpu.prog.size())
            return "ELF: text+data too large";
        if(size > 0)
            memcpy(&cpu.prog[cpu.last_addr], data.data(), size);
        cpu.last_addr += (uint16_t)size;
    }

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

        elf_data_symbol_t sym;
        sym.name = demangle(name.data());
        sym.addr = (uint16_t)addr;
        sym.size = (uint16_t)size;
        sym.color_index = 0;
        sym.global = (flags & object::SymbolRef::Flags::SF_Global) != 0;
        sym.weak   = (flags & object::SymbolRef::Flags::SF_Weak) != 0;
        sym.notype = (type == object::SymbolRef::Type::ST_Unknown);
        sym.object = (type == object::SymbolRef::Type::ST_Data);
        try_insert_symbol(is_text ? elf.text_symbols : elf.data_symbols, sym);
    }

    // assign incrementing color indices to data and text symbols
    {
        uint16_t index = 0;
        for(auto& sym : elf.data_symbols)
            sym.second.color_index = index++;
    }
    {
        uint16_t index = 0;
        for(auto& sym : elf.text_symbols)
            if(sym.second.object)
                sym.second.color_index = index++;
    }

    // sort symbols by name
    for(auto const& kv : elf.text_symbols)
        elf.text_symbols_sorted.push_back(kv.first);
    for(auto const& kv : elf.data_symbols)
        elf.data_symbols_sorted.push_back(kv.first);
    for(auto const& kv : elf.data_symbols)
        elf.data_symbols_sorted_size.push_back(kv.first);
    std::sort(elf.text_symbols_sorted.begin(), elf.text_symbols_sorted.end(),
        [&](uint16_t a, uint16_t b) { return icompare(elf.text_symbols[a].name, elf.text_symbols[b].name); });
    std::sort(elf.data_symbols_sorted.begin(), elf.data_symbols_sorted.end(),
        [&](uint16_t a, uint16_t b) { return icompare(elf.data_symbols[a].name, elf.data_symbols[b].name); });
    std::sort(elf.data_symbols_sorted_size.begin(), elf.data_symbols_sorted_size.end(),
        [&](uint16_t a, uint16_t b) { return elf.data_symbols[a].size > elf.data_symbols[b].size; });

    // note object text symbols in disassembly
    for(auto const& kv : elf.text_symbols)
    {
        auto const& sym = kv.second;
        if(!sym.object) continue;
        auto addr = sym.addr;
        auto addr_end = addr + sym.size;
        while(addr < addr_end)
        {
            auto i = cpu.addr_to_disassembled_index(addr & ~1);
            cpu.disassembled_prog[i].type = disassembled_instr_t::OBJECT;
            cpu.disassembled_prog[i].obj_bytes = (
                instr_is_two_words(cpu.decoded_prog[addr / 2]) ? 4 : 2);
            addr += 2;
        }
    }

    // load debug info
    auto dwarf_ctx = DWARFContext::create(*obj);
    load_elf_debug(a, elf, obj, dwarf_ctx.get());

    elf.obj.swap(bin_or_err.get());
    elf.dwarf_ctx.swap(dwarf_ctx);
    a.elf.swap(elf_ptr);

    return "";
}

static std::string load_elf(arduboy_t& a, std::string const& fname)
{
    std::ifstream f(fname, std::ios::binary);
    if(f.fail())
        return "ELF: Could not open file";
    return load_elf(a, f, fname);
}
#endif

static std::string load_bin(arduboy_t& a, std::istream& f, bool save)
{
    auto& d = save ? a.fxsave : a.fxdata;
    d = std::vector<uint8_t>(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
    if(a.fxdata.size() + a.fxsave.size() >= a.fx.data.size())
        return "BIN: FX data too large";

    // analyze data to determine if the binfile was a flashcart
    a.flashcart_loaded = false;
    if(save) goto not_flashcart;

    if(d.size() < 1024) goto not_flashcart;
    if(0 != memcmp(d.data() + 0x00, "ARDUBOY", 8)) goto not_flashcart;
    if(d[8] != 0xff) goto not_flashcart;
    if(d[9] != 0xff) goto not_flashcart;
    for(int i = 0x0f; i < 0x39; ++i)
        if(d[i] != 0xff) goto not_flashcart;
    if(0 != memcmp(d.data() + 0x39, "Bootloader", 10)) goto not_flashcart;
    a.flashcart_loaded = true;

not_flashcart:

    return "";
}

static std::string load_bin(arduboy_t& a, std::string const& fname, bool save)
{
    std::ifstream f(fname, std::ios::binary);
    if(f.fail())
        return "BIN: Unable to open file";
    return load_bin(a, f, save);
}

#ifndef ARDENS_NO_ARDUBOY_FILE

static std::string load_arduboy(arduboy_t& a, std::istream& f)
{
    std::vector<uint8_t> fdata(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());

    struct zip_t
    {
        mz_zip_archive z = {};
        ~zip_t() { mz_zip_reader_end(&z); }
    };
    zip_t zip;
    auto* z = &zip.z;
    mz_zip_reader_init_mem(z, fdata.data(), fdata.size(), 0);

    std::vector<char> info;
    {
        int i = mz_zip_reader_locate_file(z, "info.json", nullptr, 0);
        if(i == -1)
            return "ARDUBOY: No info.json file";
        mz_zip_archive_file_stat stat{};
        mz_zip_reader_file_stat(z, i, &stat);
        info.resize(stat.m_uncomp_size);
        mz_zip_reader_extract_to_mem(z, i, info.data(), info.size(), 0);
    }

    yyjson_read_err json_err{};
    yyjson_doc* doc;
    {
        // yyjson doesn't like BOM
        char* raw_info = info.data();
        size_t raw_size = info.size();
        if(raw_size >= 3 &&
            raw_info[0] == (char)0xef &&
            raw_info[1] == (char)0xbb &&
            raw_info[2] == (char)0xbf)
        {
            raw_size -= 3;
            raw_info += 3;
        }
        doc = yyjson_read_opts(raw_info, raw_size,
            YYJSON_READ_STOP_WHEN_DONE |
            YYJSON_READ_ALLOW_TRAILING_COMMAS |
            YYJSON_READ_ALLOW_COMMENTS |
            0,
            nullptr,
            &json_err);
    }
    if(!doc)
        return std::string("ARDUBOY: ") + json_err.msg;

    struct doc_deleter {
        yyjson_doc* doc;
        ~doc_deleter() { yyjson_doc_free(doc); }
    } doc_deleter_obj = { doc };

    yyjson_val* root = yyjson_doc_get_root(doc);
    yyjson_val* bins = yyjson_obj_get(root, "binaries");
    if(!bins)
        return "ARDUBOY: info.json missing 'binaries'";
    if(!yyjson_is_arr(bins))
        return "ARDUBOY: 'binaries' not array";
    yyjson_val* bin = yyjson_arr_get_first(bins);
    if(!bin)
        return "ARDUBOY: empty 'binaries'";
    yyjson_val* hexfile = yyjson_obj_get(bin, "filename");
    if(!hexfile)
        return "ARDUBOY: primary binary missing 'filename'";
    if(!yyjson_is_str(hexfile))
        return "ARDUBOY: primary binary filename not string type";

    a.title.clear();
    yyjson_val* title = yyjson_obj_get(root, "title");
    if(title && yyjson_is_str(title))
        a.title = yyjson_get_str(title);

    std::vector<char> data;
    {
        int i = mz_zip_reader_locate_file(z, yyjson_get_str(hexfile), nullptr, 0);
        if(i == -1)
            return "ARDUBOY: missing hex file indicated in info.json";
        mz_zip_archive_file_stat stat{};
        mz_zip_reader_file_stat(z, i, &stat);
        data.resize(stat.m_uncomp_size);
        mz_zip_reader_extract_to_mem(z, i, data.data(), data.size(), 0);
    }

    {
        std::stringstream ss(std::string(data.begin(), data.end()));
        std::string err = load_hex(a, ss);
        if(!err.empty()) return err;
    }

    a.fxdata.clear();
    a.fxsave.clear();
    a.device_type.clear();
    yyjson_val* device = yyjson_obj_get(bin, "device");
    if(device && yyjson_is_str(device))
        a.device_type = yyjson_get_str(device);
    yyjson_val* binfile = yyjson_obj_get(bin, "flashdata");
    if(binfile)
    {
        if(!yyjson_is_str(binfile))
            return "ARDUBOY: FX data filename not string type";

        {
            int i = mz_zip_reader_locate_file(z, yyjson_get_str(binfile), nullptr, 0);
            if(i == -1)
                return "ARDUBOY: missing FX data file indicated in info.json";
            mz_zip_archive_file_stat stat{};
            mz_zip_reader_file_stat(z, i, &stat);
            a.fxdata.resize(stat.m_uncomp_size);
            mz_zip_reader_extract_to_mem(z, i, a.fxdata.data(), a.fxdata.size(), 0);
        }

        // check for save bin
        yyjson_val* savefile = yyjson_obj_get(bin, "flashsave");
        if(savefile)
        {
            if(!yyjson_is_str(savefile))
                return "ARDUBOY: FX save filename not string type";

            {
                int i = mz_zip_reader_locate_file(z, yyjson_get_str(savefile), nullptr, 0);
                if(i == -1)
                    return "ARDUBOY: missing FX save file indicated in info.json";
                mz_zip_archive_file_stat stat{};
                mz_zip_reader_file_stat(z, i, &stat);
                a.fxsave.resize(stat.m_uncomp_size);
                mz_zip_reader_extract_to_mem(z, i, a.fxsave.data(), a.fxsave.size(), 0);
            }
        }

        if(a.fxdata.size() + a.fxsave.size() >= a.fx.data.size())
            return "BIN: FX data too large";
    }
    else
    {
        // this game has no FX data.
        // erase any existing data to ensure correct game hash
        a.fx.erase_all_data();
    }

    return "";
}

static std::string load_arduboy(arduboy_t& a, std::string const& fname)
{
    std::ifstream f(fname, std::ios::binary);
    if(f.fail())
        return "ARDUBOY: Unable to open file";
    return load_arduboy(a, f);
}
#endif

static bool ends_with(std::string const& str, std::string const& end)
{
    if(str.size() < end.size()) return false;
    return str.substr(str.size() - end.size()) == end;
}

// returns true if fx usage detected
static bool check_for_fx_usage_in_prog_helper(arduboy_t& a, uint8_t fxport, uint8_t fxbit)
{
    // analyze instructions to detect FX usage.
    // if none, erase FX data to ensure correct game hash
    bool found_sbi = false;
    bool found_cbi = false;
    auto const& p = a.cpu.decoded_prog;
    for(size_t i = 0; i < p.size(); ++i)
    {

        // check for instruction sequence in FX::begin
        //     lpm  r18, Z+
        //     lpm  r19, Z+
        //     lpm  r21, Z+
        //     lpm  r20, Z+
        //     subi r18, 0x18
        //     sbci r19, 0x95
        //     brne .+2
        if(i + 6 < p.size() &&
            p[i + 0].func == INSTR_LPM  && p[i + 0].word == 1 && p[i + 0].dst == 18 &&
            p[i + 1].func == INSTR_LPM  && p[i + 1].word == 1 && p[i + 1].dst == 19 &&
            p[i + 2].func == INSTR_LPM  && p[i + 2].word == 1 && p[i + 2].dst == 21 &&
            p[i + 3].func == INSTR_LPM  && p[i + 3].word == 1 && p[i + 3].dst == 20 &&
            p[i + 4].func == INSTR_SUBI && p[i + 4].dst == 18 && p[i + 4].src == 0x18 &&
            p[i + 5].func == INSTR_SBCI && p[i + 5].dst == 19 && p[i + 5].src == 0x95 &&
            p[i + 6].func == INSTR_BRBC && p[i + 6].src == 1  && p[i + 6].word == 1 &&
            true)
        {
            return true;
        }

        // check for BOTH cbi/sbi %[fxport], %[fxbit]
        {
            auto const& di = p[i];
            if(di.dst == fxport && di.src != fxbit)
            {
                if(di.func == INSTR_CBI) found_cbi = true;
                if(di.func == INSTR_SBI) found_sbi = true;
            }
            if(found_cbi && found_sbi)
                return true;
        }
    }
    a.fx.erase_all_data();
    return false;
}

static void check_for_fx_usage_in_prog(arduboy_t& a)
{
    if(check_for_fx_usage_in_prog_helper(a, 0x2b, 1 << 1))
    {
        a.device_type = "ArduboyFX";
        return;
    }
    else if(check_for_fx_usage_in_prog_helper(a, 0x2e, 1 << 2))
    {
        a.device_type = "ArduboyMini";
        return;
    }
    else if(check_for_fx_usage_in_prog_helper(a, 0x2b, 1 << 2))
    {
        a.device_type = "ArduboyFXDevKit";
        return;
    }
}

std::string arduboy_t::load_bootloader_hex(std::istream& f)
{
    return load_hex(*this, f, true);
}

std::string arduboy_t::load_bootloader_hex(uint8_t const* data, size_t size)
{
    std::istrstream f((char const*)data, (int)size);
    return load_bootloader_hex(f);
}

std::string arduboy_t::load_file(char const* filename, std::istream& f, bool save)
{
    if(f.fail())
        return "Failed to open file";

    std::string fname(filename);
    std::string r;

    if(ends_with(fname, ".save"))
    {
        if(cpu.decoded)
        {
            reset();
            if(load_savedata(f))
                savedata_dirty = true;
        }
        return "";
    }

    if(ends_with(fname, ".hex"))
    {
        flashcart_loaded = false;
        reset();
        elf.reset();
        device_type.clear();
        r = load_hex(*this, f);
        if(r.empty())
        {
            check_for_fx_usage_in_prog(*this);
            cpu.program_loaded = true;
        }
        reset();
    }

    if(ends_with(fname, ".bin"))
    {
        reset();
        r = load_bin(*this, f, save);
        reload_fx();
        if(flashcart_loaded)
        {
            cpu.program_loaded = true;
            reset();
        }
        return r;
    }

#ifdef ARDENS_LLVM
    if(ends_with(fname, ".elf"))
    {
        flashcart_loaded = false;
        reset();
        device_type.clear();
        r = load_elf(*this, f, fname);
        if(r.empty())
        {
            check_for_fx_usage_in_prog(*this);
            cpu.program_loaded = true;
        }
        reset();
}
#endif

#ifndef ARDENS_NO_ARDUBOY_FILE
    if(ends_with(fname, ".arduboy"))
    {
        flashcart_loaded = false;
        reset();
        elf.reset();
        device_type.clear();
        r = load_arduboy(*this, f);
        reload_fx();
        if(r.empty())
            cpu.program_loaded = true;
        reset();
    }
#endif

#ifndef ARDENS_NO_SNAPSHOTS
    if(ends_with(fname, ".snapshot"))
    {
        r = load_snapshot(f);
    }
    else
#endif
    if(r.empty())
    {
        f.clear();
        f.seekg(0);
        prog_filename = filename;
        prog_filedata = std::vector<uint8_t>(
            (std::istreambuf_iterator<char>(f)),
            std::istreambuf_iterator<char>());
        reload_fx();
    }

    return r;
}

}
