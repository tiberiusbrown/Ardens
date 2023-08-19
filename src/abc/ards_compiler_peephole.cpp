#include "ards_compiler.hpp"

namespace ards
{

bool compiler_t::peephole(compiler_func_t& f)
{
    bool t = false;
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

    }
    return t;
}

}
