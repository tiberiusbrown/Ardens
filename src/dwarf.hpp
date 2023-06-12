#pragma once

#ifdef ABSIM_LLVM

#include <string>
#include <vector>

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
