#include "absim.hpp"

#include "absim_instructions.hpp"

namespace absim
{

static ABSIM_FORCEINLINE void set_flag(atmega32u4_t& cpu, uint8_t mask, uint32_t x)
{
    if(x) cpu.sreg() |= mask;
    else cpu.sreg() &= ~mask;
}

static ABSIM_FORCEINLINE void set_flag_s(atmega32u4_t& cpu)
{
    uint8_t f = cpu.sreg();
    set_flag(cpu, SREG_S, (f ^ (f >> 1)) & 0x4);
}

static ABSIM_FORCEINLINE void set_flags_hcv(atmega32u4_t& cpu, uint8_t h, uint8_t c, uint8_t v)
{
    set_flag(cpu, SREG_H, h);
    set_flag(cpu, SREG_C, c);
    set_flag(cpu, SREG_V, v);
}

static ABSIM_FORCEINLINE void set_flags_nzs(atmega32u4_t& cpu, uint16_t x)
{
    set_flag(cpu, SREG_N, x & 0x80);
    set_flag(cpu, SREG_Z, x == 0);
    set_flag_s(cpu);
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

}
