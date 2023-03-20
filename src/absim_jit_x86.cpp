#include "absim.hpp"

#include <asmjit/x86.h>

#if ABSIM_JIT && (ABSIM_X86 || ABSIM_X64)

namespace absim
{

static asmjit::x86::Gp jit_cpu_ptr(asmjit::x86::Compiler& cc, atmega32u4_t& cpu)
{
    auto ptr = cc.newGpz();
#if ABSIM_X86
    cc.mov(ptr, uintptr_t(&cpu));
#else
    cc.movabs(ptr, uintptr_t(&cpu));
#endif
    return ptr;
}

static void jit_instr_nop(asmjit::x86::Compiler& cc, atmega32u4_t& cpu, avr_instr_t const& i)
{
    auto r = cc.newGpd();
    auto ptr = jit_cpu_ptr(cc, cpu);
    cc.add(asmjit::x86::ptr(ptr, offsetof(atmega32u4_t, pc), 2), 1);
    cc.mov(r, 1);
    cc.ret(r);
}

static void jit_instr_ldi(asmjit::x86::Compiler& cc, atmega32u4_t& cpu, avr_instr_t const& i)
{
    auto r = cc.newGpd();
    auto ptr = jit_cpu_ptr(cc, cpu);
    cc.mov(asmjit::x86::ptr(ptr, offsetof(atmega32u4_t, data) + i.dst, 1), i.src);
    cc.add(asmjit::x86::ptr(ptr, offsetof(atmega32u4_t, pc), 2), 1);
    cc.mov(r, 1);
    cc.ret(r);
}


static bool jit_dispatch(asmjit::x86::Compiler& cc, atmega32u4_t& cpu, avr_instr_t const& i)
{
    switch(i.func)
    {
    case INSTR_NOP: jit_instr_nop(cc, cpu, i); break;
    case INSTR_LDI: jit_instr_ldi(cc, cpu, i); break;
    default:
        return false;
    }
    return true;
}

void atmega32u4_t::jit_compile()
{
    jit_runtime = std::make_unique<asmjit::JitRuntime>();
    memset(&jit_prog, 0, sizeof(jit_prog));

    auto& rt = *jit_runtime;

    for(uint32_t n = 0; n < num_instrs; ++n)
    {
        avr_instr_jit_t& i = jit_prog[n];

        asmjit::CodeHolder code;
        code.init(rt.environment(), rt.cpuFeatures());
        asmjit::x86::Compiler cc(&code);

        cc.addFunc(asmjit::FuncSignatureT<uint32_t>());

        bool found = jit_dispatch(cc, *this, decoded_prog[n]);
        
        if(found)
        {
            cc.endFunc();
            cc.finalize();

            auto err = rt.add(&i, &code);
            if(err) i = nullptr;
        }
    }



}
    
}

#endif
