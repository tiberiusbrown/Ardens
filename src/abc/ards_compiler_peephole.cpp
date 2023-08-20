#include "ards_compiler.hpp"

#include <algorithm>

namespace ards
{

bool compiler_t::peephole(compiler_func_t& f)
{
    bool t = false;

    {
        auto end = std::remove_if(
            f.instrs.begin(), f.instrs.end(),
            [](compiler_instr_t const& i) { return i.instr == I_REMOVE; }
        );
        f.instrs.erase(end, f.instrs.end());
    }

    for(size_t i = 0; i < f.instrs.size(); ++i)
    {
        auto& i0 = f.instrs[i + 0];

        if(i + 1 >= f.instrs.size()) continue;

        auto& i1 = f.instrs[i + 1];

        // replace PUSH 1; GETLN <N> with GETL <N>
        if(i0.instr == I_PUSH && i0.imm == 1)
        {
            if(i1.instr == I_GETLN)
            {
                i0.instr = I_REMOVE;
                i1.instr = I_GETL;
                t = true;
                continue;
            }
            if(i1.instr == I_SETLN)
            {
                i0.instr = I_REMOVE;
                i1.instr = I_SETL;
                t = true;
                continue;
            }
            if(i1.instr == I_GETGN)
            {
                i0.instr = I_REMOVE;
                i1.instr = I_GETG;
                t = true;
                continue;
            }
            if(i1.instr == I_SETGN)
            {
                i0.instr = I_REMOVE;
                i1.instr = I_SETG;
                t = true;
                continue;
            }
        }

        // replace PUSH 0 with P0
        if(i0.instr == I_PUSH)
        {
            if(i0.imm == 0 && i1.instr == I_PUSH && i1.imm == 0)
            {
                i0.instr = I_P00;
                i1.instr = I_REMOVE;
                t = true;
                continue;
            }
            static std::unordered_map<uint32_t, instr_t> const push_instrs =
            {
                { 0, I_P0 },
                { 1, I_P1 },
                { 2, I_P2 },
                { 3, I_P3 },
                { 4, I_P4 },
            };
            auto it = push_instrs.find(i0.imm);
            if(it != push_instrs.end())
            {
                i0.instr = it->second;
                t = true;
                continue;
            }
        }
    }
    return t;
}

}
