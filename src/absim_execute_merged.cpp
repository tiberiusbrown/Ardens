#include "absim.hpp"

#include "absim_instructions.hpp"

namespace absim
{

static ARDENS_FORCEINLINE void set_flag(atmega32u4_t& cpu, uint8_t mask, uint32_t x)
{
    if(x) cpu.sreg() |= mask;
    else cpu.sreg() &= ~mask;
}

static ARDENS_FORCEINLINE void set_flag_s(atmega32u4_t& cpu)
{
    uint8_t f = cpu.sreg();
    set_flag(cpu, SREG_S, (f ^ (f >> 1)) & 0x4);
}

static ARDENS_FORCEINLINE void set_flags_hcv(atmega32u4_t& cpu, uint8_t h, uint8_t c, uint8_t v)
{
    set_flag(cpu, SREG_H, h);
    set_flag(cpu, SREG_C, c);
    set_flag(cpu, SREG_V, v);
}

static ARDENS_FORCEINLINE void set_flags_nzs(atmega32u4_t& cpu, uint16_t x)
{
    set_flag(cpu, SREG_N, x & 0x80);
    set_flag(cpu, SREG_Z, x == 0);
    set_flag_s(cpu);
}

static ARDENS_FORCEINLINE uint8_t flag_s(uint8_t sreg)
{
    sreg |= (((sreg ^ (sreg >> 1)) & 0x4) << 2);
    return sreg;
}

static ARDENS_FORCEINLINE uint8_t flags_nzs(uint8_t sreg, uint32_t x)
{
    sreg |= ((x & 0x80) >> 5); // N
    if(x == 0) sreg |= SREG_Z; // Z
    sreg = flag_s(sreg);      // S
    return sreg;
}

uint32_t instr_merged_push_n(atmega32u4_t& cpu, avr_instr_t const& i)
{
    auto* ip = &i;
    uint32_t n = i.word;
    uint32_t r = 0;
    do
    {
        cpu.push(cpu.gpr(ip->src));
        --n;
        ++ip;
        r += 2;
        cpu.pc += 1;
    } while(!cpu.should_autobreak() && n != 0);
    return r;
}

uint32_t instr_merged_pop_n(atmega32u4_t& cpu, avr_instr_t const& i)
{
    auto* ip = &i;
    uint32_t n = i.word;
    uint32_t r = n * 2;
    cpu.pc += n;
    do
    {
        cpu.gpr(ip->src) = cpu.pop();
        --n;
        ++ip;
    } while(n != 0);
    return r;
}

uint32_t instr_merged_ldi_n(atmega32u4_t& cpu, avr_instr_t const& i)
{
    auto* ip = &i;
    uint32_t n = i.word;
    uint32_t r = n;
    cpu.pc += n;
    do
    {
        cpu.gpr(ip->dst) = ip->src;
        --n;
        ++ip;
    } while(n != 0);
    return r;
}

uint32_t instr_merged_dec_brne(atmega32u4_t& cpu, avr_instr_t const& i)
{
    auto const& ib = *(&i + 1);
    uint8_t dst = cpu.gpr(i.dst);
    uint8_t res = dst - 1;
    cpu.gpr(i.dst) = res;
    set_flag(cpu, SREG_V, dst == 0x80);
    set_flags_nzs(cpu, res);

    if(res != 0)
    {
        cpu.pc += (int16_t)ib.word + 2;
        return 3;
    }

    cpu.pc += 2;
    return 2;
}

uint32_t instr_merged_add_adc(atmega32u4_t& cpu, avr_instr_t const& i)
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

}
