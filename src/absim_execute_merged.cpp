#include "absim.hpp"

#include "absim_instructions.hpp"

namespace absim
{

ARDENS_FORCEINLINE static uint8_t flag_s(uint8_t sreg)
{
    // S |= N ^ V
    sreg |= (((sreg ^ (sreg >> 1)) & 0x4) << 2);
    return sreg;
}

ARDENS_FORCEINLINE static uint8_t flags_nzs(uint8_t sreg, uint32_t x)
{
    sreg |= ((x & 0x80) >> 5); // N
    if(x == 0) sreg |= SREG_Z; // Z
    sreg = flag_s(sreg);       // S
    return sreg;
}

ARDENS_FORCEINLINE static uint8_t flags_nzs16(uint8_t sreg, uint32_t x)
{
    sreg |= ((x & 0x8000) >> 13); // N
    if(x == 0) sreg |= SREG_Z; // Z
    sreg = flag_s(sreg);       // S
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
    unsigned sreg = cpu.sreg() & ~(SREG_V | SREG_N | SREG_Z | SREG_S);
    if(dst == 0x80) sreg |= SREG_V;
    sreg = flags_nzs(sreg, res);
    cpu.sreg() = sreg;

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
    sreg = flags_nzs16(sreg, res);
    cpu.sreg() = (uint8_t)sreg;

    cpu.pc += 2;
    return 2;
}

uint32_t instr_merged_sub_sbc(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned dst = cpu.gpr(i.dst) + cpu.gpr(i.dst + 1) * 256;
    unsigned src = cpu.gpr(i.src) + cpu.gpr(i.src + 1) * 256;
    unsigned res = (dst - src) & 0xffff;
    cpu.gpr(i.dst + 0) = (uint8_t)(res >> 0);
    cpu.gpr(i.dst + 1) = (uint8_t)(res >> 8);

    unsigned sreg = cpu.sreg() & ~SREG_HSVNZC;
    unsigned hc = (~dst & src) | (src & res) | (res & ~dst);
    unsigned v = (dst & ~src & ~res) | (~dst & src & res);
    sreg |= (hc & 0x0800) >> 6;  // H flag
    sreg |= hc >> 15;            // C flag
    sreg |= (v & 0x8000) >> 12;  // V flag
    sreg = flags_nzs16(sreg, res);
    cpu.sreg() = (uint8_t)sreg;

    cpu.pc += 2;
    return 2;
}

uint32_t instr_merged_cp_cpc(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned dst = cpu.gpr(i.dst) + cpu.gpr(i.dst + 1) * 256;
    unsigned src = cpu.gpr(i.src) + cpu.gpr(i.src + 1) * 256;
    unsigned res = (dst - src) & 0xffff;

    unsigned sreg = cpu.sreg() & ~SREG_HSVNZC;
    unsigned hc = (~dst & src) | (src & res) | (res & ~dst);
    unsigned v = (dst & ~src & ~res) | (~dst & src & res);
    sreg |= (hc & 0x0800) >> 6;  // H flag
    sreg |= hc >> 15;            // C flag
    sreg |= (v & 0x8000) >> 12;  // V flag
    sreg = flags_nzs16(sreg, res);
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
    sreg = flags_nzs16(sreg, res);
    cpu.sreg() = (uint8_t)sreg;

    cpu.pc += 2;
    return 2;
}

uint32_t instr_merged_delay(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.pc += i.src;
    return i.word;
}

}
