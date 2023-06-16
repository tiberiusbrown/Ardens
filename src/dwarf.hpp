#pragma once

#ifdef ABSIM_LLVM

#include <string>
#include <vector>
#include <array>
#include <assert.h>

#ifdef _MSC_VER
#pragma warning(push, 1)
#pragma warning(disable: 4624)
#endif

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFCompileUnit.h>
#include <llvm/DebugInfo/DWARF/DWARFExpression.h>

#ifdef _MSC_VER
#pragma warning(pop) 
#endif

struct dwarf_span
{
    uint8_t const* begin;
    uint8_t const* end;
    dwarf_span offset(size_t offset)
    {
        return { begin + offset, end };
    }
    uint8_t operator[](size_t i) const
    {
        assert(begin + i < end);
        return begin[i];
    }
    size_t size() const
    {
        return end < begin ? 0 : size_t(end - begin);
    }
};
template<size_t N> dwarf_span to_dwarf_span(std::array<uint8_t, N> const& a)
{
    return dwarf_span{ a.data(), a.data() + a.size() };
}
template<class... R> dwarf_span to_dwarf_span(std::vector<uint8_t, R...> const& v)
{
    return dwarf_span{ v.data(), v.data() + v.size() };
}

uint32_t dwarf_size(llvm::DWARFDie die);
std::string dwarf_name(llvm::DWARFDie die);
llvm::DWARFDie dwarf_type(llvm::DWARFDie die);

struct dwarf_member_t
{
    uint32_t offset;
    uint32_t bit_offset;
    uint32_t bit_size;
    llvm::DWARFDie die;
};
std::vector<dwarf_member_t> dwarf_members(llvm::DWARFDie die);

// on success, add member offset to addr and return true
// on failure, return false
bool dwarf_member_addr(
    uint32_t& addr, uint32_t& bit_offset, uint32_t& bit_size,
    llvm::DWARFDie die);

std::vector<uint32_t> dwarf_array_bounds(llvm::DWARFDie die);

std::vector<uint32_t> dwarf_array_index(llvm::DWARFDie die, uint32_t index);

std::string dwarf_type_string(llvm::DWARFDie die);

std::string dwarf_value_string(
    llvm::DWARFDie die, uint32_t addr, bool prog,
    uint32_t bit_offset = 0, uint32_t bit_size = 0);

std::string dwarf_value_string(
    llvm::DWARFDie die, dwarf_span mem,
    uint32_t bit_offset = 0, uint32_t bit_size = 0);

struct dwarf_var_data
{
    std::vector<uint8_t> data;
    std::vector<uint8_t> unavailable; // mask: 1 bits mean optimized out

    // int: how many bytes
    // str: expression for those bytes
    std::vector<std::pair<int, std::string>> formulas;
};

dwarf_var_data dwarf_evaluate_location(
    llvm::DWARFDie type,
    llvm::StringRef loc);

struct dwarf_primitive_t
{
    llvm::DWARFDie die;
    uint32_t offset;
    uint32_t bit_offset;
    uint32_t bit_size;
    std::string expr;
};
// on success, find the die for the primitive member type at offset
bool dwarf_find_primitive(llvm::DWARFDie die, uint32_t offset, dwarf_primitive_t& prim);

std::string dwarf_function_args_string(llvm::DWARFDie die);

#endif
