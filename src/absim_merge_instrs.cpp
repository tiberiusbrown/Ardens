#include "absim.hpp"

// possibilities:

//     mul[s][u] + movw rN, r0 + eor r1, r1
//     ldi rC, N + add rA, rA + adc rB, rB + dec RC + brne
//     ldi rC, N + lsr rA, rA + ror rB, rB + dec RC + brne
//     ldi rC, N + asr rA, rA + ror rB, rB + dec RC + brne
//     lsr rN + lsr rN + ... + lsr rN
//     add + adc
//     subi + sbci
//     eor rN, rN + dec rN
//     and rN, rN + brbc
//     and rN, rN + brbs
//     and + or (used in sprite drawing)
//     sbiw rN, 1 + brne

// ymask:
//     ldi  r19, 0x01
//     sbrc r22, 1
//     ldi  r19, 0x04
//     sbrc r22, 0
//     add  r19, r19
//     sbrc r22, 2
//     swap r19

namespace absim
{

static uint32_t instr_is_delay(atmega32u4_t const& cpu, size_t n)
{
    auto i = cpu.decoded_prog[n];
    switch(i.func)
    {
    case INSTR_NOP:
        return 1;
    case INSTR_RJMP:
    {
        if(i.word == 0)
            return 2;
        size_t nr = n + i.word + 1;
        if(nr < cpu.decoded_prog.size() && cpu.decoded_prog[nr].func == INSTR_RET)
            return 7;
    }
    default:
        return 0;
    }
}

void atmega32u4_t::merge_instrs()
{
    memcpy(merged_prog.data(), &decoded_prog, array_bytes(merged_prog));

    for(size_t n = 0; n + 1 < merged_prog.size(); ++n)
    {
        auto& i0 = merged_prog[n + 0];
        auto  i1 = decoded_prog[n + 1];

        if(i0.func == INSTR_PUSH)
        {
            uint32_t t;
            for(t = 1; t < 4; ++t)
            {
                if(n + t >= decoded_prog.size()) break;
                if(decoded_prog[n + t].func != INSTR_PUSH) break;
            }
            if(t >= 4)
            {
                i0.func = INSTR_MERGED_PUSH4;
                i0.m0 = i1.src;
                i0.m1 = decoded_prog[n + 2].src;
                i0.m2 = decoded_prog[n + 3].src;
            }
            continue;
        }

        if(i0.func == INSTR_POP)
        {
            uint32_t t;
            for(t = 1; t < 4; ++t)
            {
                if(n + t >= decoded_prog.size()) break;
                if(decoded_prog[n + t].func != INSTR_POP) break;
            }
            if(t >= 4)
            {
                i0.func = INSTR_MERGED_POP4;
                i0.m0 = i1.src;
                i0.m1 = decoded_prog[n + 2].src;
                i0.m2 = decoded_prog[n + 3].src;
            }
            continue;
        }

        if(i0.func == INSTR_LDI && i1.func == INSTR_LDI)
        {
            i0.func = INSTR_MERGED_LDI2;
            i0.m0 = i1.dst;
            i0.m1 = i1.src;
            continue;
        }

        if(i0.func == INSTR_DEC && i1.func == INSTR_BRBC && i1.src == 1)
        {
            i0.func = INSTR_MERGED_DEC_BRNE;
            i0.word = i1.word;
            continue;
        }

        if(i0.func == INSTR_ADD &&
            i1.func == INSTR_ADC &&
            i0.dst + 1 == i1.dst &&
            i0.src + 1 == i1.src)
        {
            i0.func = INSTR_MERGED_ADD_ADC;
            continue;
        }

        if(i0.func == INSTR_SUB &&
            i1.func == INSTR_SBC &&
            i0.dst + 1 == i1.dst &&
            i0.src + 1 == i1.src)
        {
            i0.func = INSTR_MERGED_SUB_SBC;
            continue;
        }

        if(i0.func == INSTR_CP &&
            i1.func == INSTR_CPC &&
            i0.dst + 1 == i1.dst &&
            i0.src + 1 == i1.src)
        {
            i0.func = INSTR_MERGED_CP_CPC;
            continue;
        }

        if(i0.func == INSTR_SUBI &&
            i1.func == INSTR_SBCI)
        {
            i0.func = INSTR_MERGED_SUBI_SBCI;
            i0.word = i0.src + i1.src * 256;
            i0.src = i1.dst;
            continue;
        }

        {
            uint32_t d = 0;
            uint32_t n = 0;
            for(size_t m = n; n < 254 && m < decoded_prog.size(); ++n)
            {
                uint32_t t = instr_is_delay(*this, m);
                if(t == 0) break;
                d += t;
                n += instr_is_two_words(decoded_prog[m]) ? 2 : 1;
            }
            if(d > 1)
            {
                i0.func = INSTR_MERGED_DELAY;
                i0.src = (uint8_t)n;
                i0.word = (uint16_t)d;
            }
        }
    }
}

}
