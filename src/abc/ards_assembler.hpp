#pragma once

#include <istream>
#include <string>
#include <unordered_map>
#include <vector>

#include <limits.h>

#include "ards_error.hpp"
#include "ards_vm/ards_instr.hpp"

namespace ards
{

// map from sys function names to indices
extern std::unordered_map<std::string, sysfunc_t> const sys_names;

struct assembler_t
{
    assembler_t()
        : globals_bytes(0)
        , byte_count(
            4 + // signature
            4 + // CALL main
            4 + // JMP 0x000004
            0)
        , error{}
    {}
    error_t assemble(std::istream& f);
    error_t link();
    std::vector<uint8_t> const& data() { return linked_data; }
private:
    std::vector<uint8_t> linked_data;
    
    // convert label name to node index
    std::unordered_map<std::string, size_t> labels;

    // convert label name to global data offset
    std::unordered_map<std::string, size_t> globals;

    // current amount of global data bytes
    size_t globals_bytes;
    
    enum node_type_t : uint8_t
    {
        INSTR,  // instruction opcode
        LABEL,  // label reference
        GLOBAL, // global label reference
        IMM,    // immediate value
    };
    
    struct node_t
    {
        size_t offset;
        node_type_t type;
        instr_t instr;
        uint16_t size; // size of object in bytes
        uint32_t imm;  // also used for label offset
        std::string label;
    };
    
    std::vector<node_t> nodes;
    size_t byte_count;
    error_t error;
    
    void push_instr(instr_t i)
    {
        nodes.push_back({ byte_count, INSTR, i, 1 });
        byte_count += 1;
    }

    void push_imm(uint32_t i, uint16_t size)
    {
        nodes.push_back({ byte_count, IMM, I_NOP, size, i });
        byte_count += size;
    }

    void push_label(std::string const& label)
    {
        nodes.push_back({ byte_count, LABEL, I_NOP, 3, 0, label });
        byte_count += 3;
    }

    void push_global(std::istream& f);

};
    
}
