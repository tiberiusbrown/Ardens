#include "absim.hpp"

#include "absim_instructions.hpp"

namespace absim
{

ARDENS_FORCEINLINE static void set_flag(atmega32u4_t& cpu, uint8_t mask, uint32_t x)
{
    if(x) cpu.sreg() |= mask;
    else cpu.sreg() &= ~mask;
}

ARDENS_FORCEINLINE static void set_flag_s(atmega32u4_t& cpu)
{
    uint8_t f = cpu.sreg();
    set_flag(cpu, SREG_S, (f ^ (f >> 1)) & 0x4);
}

ARDENS_FORCEINLINE static void set_flags_hcv(atmega32u4_t& cpu, uint8_t h, uint8_t c, uint8_t v)
{
    set_flag(cpu, SREG_H, h);
    set_flag(cpu, SREG_C, c);
    set_flag(cpu, SREG_V, v);
}

ARDENS_FORCEINLINE static void set_flags_nzs(atmega32u4_t& cpu, uint16_t x)
{
    set_flag(cpu, SREG_N, x & 0x80);
    set_flag(cpu, SREG_Z, x == 0);
    set_flag_s(cpu);
}

ARDENS_FORCEINLINE static uint8_t flag_s(uint8_t sreg)
{
    sreg |= (((sreg ^ (sreg >> 1)) & 0x4) << 2);
    return sreg;
}

ARDENS_FORCEINLINE static uint8_t flags_nzs(uint8_t sreg, uint32_t x)
{
    sreg |= ((x & 0x80) >> 5); // N
    if(x == 0) sreg |= SREG_Z; // Z
    sreg = flag_s(sreg);      // S
    return sreg;
}

uint32_t instr_merged_push4(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.push(cpu.gpr(i.src));
    if(cpu.should_autobreak()) { cpu.pc += 1; return 2; }
    cpu.push(cpu.gpr(i.m0));
    if(cpu.should_autobreak()) { cpu.pc += 2; return 4; }
    cpu.push(cpu.gpr(i.m1));
    if(cpu.should_autobreak()) { cpu.pc += 3; return 6; }
    cpu.push(cpu.gpr(i.m2));
    cpu.pc += 4;
    return 8;
}

uint32_t instr_merged_pop4(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.gpr(i.src) = cpu.pop();
    cpu.gpr(i.m0 ) = cpu.pop();
    cpu.gpr(i.m1 ) = cpu.pop();
    cpu.gpr(i.m2 ) = cpu.pop();
    cpu.pc += 4;
    return 8;
}

uint32_t instr_merged_ldi2(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.gpr(i.dst) = i.src;
    cpu.gpr(i.m0 ) = i.m1;
    cpu.pc += 2;
    return 2;
}

uint32_t instr_merged_dec_brne(atmega32u4_t& cpu, avr_instr_t i)
{
    uint8_t dst = cpu.gpr(i.dst);
    uint8_t res = dst - 1;
    cpu.gpr(i.dst) = res;
    set_flag(cpu, SREG_V, dst == 0x80);
    set_flags_nzs(cpu, res);

    if(res != 0)
    {
        cpu.pc += (int16_t)i.word + 2;
        return 3;
    }

    cpu.pc += 2;
    return 2;
}

uint32_t instr_merged_add_adc(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned dst = cpu.gpr(i.dst) + cpu.gpr(i.dst + 1) * 256;
    unsigned src = cpu.gpr(i.src) + cpu.gpr(i.src + 1) * 256;
    unsigned res = (dst + src) & 0xffff;
    cpu.gpr(i.dst + 0) = (uint8_t)(res >> 0);
    cpu.gpr(i.dst + 1) = (uint8_t)(res >> 8);

    unsigned hc = (dst & src) | (src & ~res) | (~res & dst);
    unsigned v = (dst & src & ~res) | (~dst & ~src & res);
    unsigned sreg = cpu.sreg() & ~SREG_HSVNZC;
    sreg |= (hc & 0x0800) >> 6;  // H flag
    sreg |= hc >> 15;            // C flag
    sreg |= (v & 0x8000) >> 12;  // V flag
    sreg = flags_nzs(sreg, res);
    cpu.sreg() = (uint8_t)sreg;

    cpu.pc += 2;
    return 2;
}

uint32_t instr_merged_subi_sbci(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned dst = cpu.gpr(i.dst) + cpu.gpr(i.src) * 256;
    unsigned src = i.word;
    unsigned res = (dst - src) & 0xffff;
    cpu.gpr(i.dst) = (uint8_t)(res >> 0);
    cpu.gpr(i.src) = (uint8_t)(res >> 8);

    unsigned sreg = cpu.sreg() & ~SREG_HSVNZC;
    unsigned hc = (~dst & src) | (src & res) | (res & ~dst);
    unsigned v = (dst & ~src & ~res) | (~dst & src & res);
    sreg |= (hc & 0x0800) >> 6;  // H flag
    sreg |= hc >> 15;            // C flag
    sreg |= (v & 0x8000) >> 12;  // V flag
    sreg = flags_nzs(sreg, res);
    cpu.sreg() = (uint8_t)sreg;

    cpu.pc += 2;
    return 2;
}

}
