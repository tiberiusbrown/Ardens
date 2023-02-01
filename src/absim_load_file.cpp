#include "absim.hpp"

#include <string>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <cctype>

#include <elfio/elfio.hpp>
#include <nlohmann/json.hpp>
#include <zip_file.hpp>

#ifdef _MSC_VER
#pragma warning(push, 1)
#pragma warning(disable: 4624)
#endif

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFCompileUnit.h>
#include <llvm/Demangle/Demangle.h>

#ifdef _MSC_VER
#pragma warning(pop) 
#endif

// trim from start (in place)
static inline void ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
}

// trim from end (in place)
static inline void rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s)
{
    rtrim(s);
    ltrim(s);
}

struct MyDWARFObject : public llvm::DWARFObject
{
    llvm::StringRef    d_aranges;
    llvm::DWARFSection d_info;
    llvm::StringRef    d_abbrev;
    llvm::DWARFSection d_line;
    llvm::DWARFSection d_frame;
    llvm::StringRef    d_str;
    llvm::DWARFSection d_loc;
    llvm::DWARFSection d_ranges;

    // TODO: set this from ELF file
    bool isLittleEndian() const { return true; }

    void forEachInfoSections(
        llvm::function_ref<void(llvm::DWARFSection const&)> F) const override
    {
        F(d_info);
    }

    llvm::StringRef getArangesSection() const override { return d_aranges; }
    llvm::StringRef getAbbrevSection() const override { return d_abbrev; }
    llvm::StringRef getStrSection() const override { return d_str; }
    llvm::DWARFSection const& getLineSection() const override { return d_line; }
    llvm::DWARFSection const& getFrameSection() const override { return d_frame; }
    llvm::DWARFSection const& getLocSection() const override { return d_loc; }
    llvm::DWARFSection const& getRangesSection() const override { return d_ranges; }

    llvm::Optional<llvm::RelocAddrEntry> find(
        llvm::DWARFSection const& sec, uint64_t Pos) const override
    {
        return {};
    }

    virtual ~MyDWARFObject() = default;
};

//extern "C" char* __cxa_demangle(const char* MangledName, char* Buf, size_t * N, int* Status);

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
    if(sym.bind == 2) return;
    auto& oldsym = it->second;
    // if new is NOTYPE and old is not NOTYPE don't replace
    if(sym.type == 0 && oldsym.type != 0) return;
    // if old is GLOBAL don't replace it
    if(oldsym.bind == 1) return;
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
        auto& v = elf.source_files.back();
        elf.source_file_names[filename] = i;
        std::string linestr;
        while(std::getline(f, linestr))
        {
            trim(linestr);
            v.push_back(linestr);
        }
        elf.source_lines[addr] = { i, line };
    }
    else
        elf.source_lines[addr] = { elf.source_file_names[filename], line };
}

static char const* load_elf(arduboy_t& a, std::string const& fname)
{
    using namespace ELFIO;
    elfio reader;

    if(!reader.load(fname))
        return "ELF: Unable to load file";

    auto& cpu = a.cpu;

    a.elf.reset();
    memset(&cpu.prog, 0, sizeof(cpu.prog));
    memset(&cpu.decoded_prog, 0, sizeof(cpu.decoded_prog));
    memset(&cpu.disassembled_prog, 0, sizeof(cpu.disassembled_prog));
    memset(&cpu.breakpoints, 0, sizeof(cpu.breakpoints));
    memset(&cpu.breakpoints_rd, 0, sizeof(cpu.breakpoints_rd));
    memset(&cpu.breakpoints_wr, 0, sizeof(cpu.breakpoints_wr));

    if(reader.get_machine() != 0x0053)
        return "ELF: machine not EM_AVR";

    if(reader.get_class() != 0x1)
        return "ELF: not a 32-bit ELF file";

    auto elf_ptr = std::make_unique<elf_data_t>();
    auto& elf = *elf_ptr;

    bool found_text = false;

    section const* sec_symtab = nullptr;
    section const* sec_strtab = nullptr;

    int sec_index_data = -1;
    int sec_index_bss = -1;
    int sec_index_text = -1;

    constexpr uint32_t ram_offset = 0x800000;

    auto dwarf = std::make_unique<MyDWARFObject>();

    for(unsigned i = 0; i < reader.sections.size(); ++i)
    {
        auto const* sec = reader.sections[i];
        auto name = sec->get_name();
        auto size = sec->get_size();
        auto data = sec->get_data();
        auto addr = sec->get_address();

        if(name == ".text")
        {
            sec_index_text = (int)i;
            if(size > cpu.PROG_SIZE_BYTES)
                return "ELF: Section .text too large";
            memcpy(&cpu.prog, data, size);
            cpu.last_addr = (uint16_t)size;
            found_text = true;
        }

        if(name == ".data")
        {
            sec_index_data = (int)i;
            elf.data_begin = uint16_t(addr - ram_offset);
            elf.data_end = uint16_t(elf.data_begin + size);
        }

        if(name == ".bss")
        {
            sec_index_bss = (int)i;
            elf.bss_begin = uint16_t(addr - ram_offset);
            elf.bss_end = uint16_t(elf.data_begin + size);
        }

        if(name == ".symtab") sec_symtab = sec;
        if(name == ".strtab") sec_strtab = sec;

        if(name == ".debug_info"  ) dwarf->d_info   = { { data, size }, addr };
        if(name == ".debug_line"  ) dwarf->d_line   = { { data, size }, addr };
        if(name == ".debug_frame" ) dwarf->d_frame  = { { data, size }, addr };
        if(name == ".debug_loc"   ) dwarf->d_loc    = { { data, size }, addr };
        if(name == ".debug_ranges") dwarf->d_ranges = { { data, size }, addr };

        if(name == ".debug_aranges") dwarf->d_aranges = { data, size };
        if(name == ".debug_abbrev" ) dwarf->d_abbrev  = { data, size };
        if(name == ".debug_str"    ) dwarf->d_str     = { data, size };
    }

    if(!found_text)
        return "ELF: No .text section";

    if(sec_symtab && sec_strtab && sec_index_data >= 0 && sec_index_bss >= 0)
    {
        char const* str = sec_strtab->get_data();
        int num_syms = (int)sec_symtab->get_size() / 16;
        Elf32_Sym const* ptr = (Elf32_Sym const*)sec_symtab->get_data();
        for(int i = 0; i < num_syms; ++i, ++ptr)
        {
            uint8_t sym_type = ptr->st_info & 0xf;
            uint8_t sym_bind = ptr->st_info >> 4;
            // limit to OBJECT or FUNC or NOTYPE
            if(sym_type > 2)
                continue;
            auto sec_index = ptr->st_shndx;
            bool text = false;
            if(sec_index == sec_index_text)
                text = true;
            else if(sec_index != sec_index_data && sec_index != sec_index_bss)
                continue;
            if(sym_type == 0 && !text)
                continue;
            char const* name = &str[ptr->st_name];
            elf_data_symbol_t sym;
            sym.name = demangle(name);
            auto addr = ptr->st_value;
            if(!text) addr -= ram_offset;
            sym.addr = (uint16_t)addr;
            sym.size = (uint16_t)ptr->st_size;
            sym.type = sym_type;
            sym.bind = sym_bind;
            try_insert_symbol(text ? elf.text_symbols : elf.data_symbols, sym);
        }
    }

    cpu.decode();

    // note object text symbols in disassembly
    for(auto const& kv : elf.text_symbols)
    {
        auto const& sym = kv.second;
        if(sym.type != 1) continue;
        auto addr = sym.addr;
        auto addr_end = addr + sym.size;
        while(addr < addr_end)
        {
            auto i = cpu.addr_to_disassembled_index(addr);
            cpu.disassembled_prog[i].type = disassembled_instr_t::OBJECT;
            addr += 2;
        }
    }

    auto dwarf_ctx = std::make_unique<llvm::DWARFContext>(std::move(dwarf));

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

    // create intermixed asm with source
    {
        auto& v = elf.asm_with_source;
        for(uint16_t i = 0; i < cpu.num_instrs; ++i)
        {
            uint16_t addr = cpu.disassembled_prog[i].addr;
            auto it = elf.source_lines.find(addr);
            if(it != elf.source_lines.end())
            {
                disassembled_instr_t instr;
                instr.addr = addr;
                instr.type = disassembled_instr_t::SOURCE;
                v.push_back(instr);
            }
            v.push_back(cpu.disassembled_prog[i]);
        }
    }

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
