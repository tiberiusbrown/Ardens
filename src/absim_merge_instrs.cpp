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

void atmega32u4_t::merge_instrs()
{
    memcpy(merged_prog.data(), &decoded_prog, array_bytes(merged_prog));

    for(size_t n = 0; n + 1 < merged_prog.size(); ++n)
    {
        auto& i0 = merged_prog[n + 0];
        auto& i1 = merged_prog[n + 1];

        if(i0.func == INSTR_PUSH)
        {
            uint32_t t;
            for(t = 1; t < MAX_INSTR_CYCLES / 2; ++t)
            {
                if(n + t >= merged_prog.size()) break;
                if(merged_prog[n + t].func != INSTR_PUSH) break;
            }
            if(t > 1)
            {
                i0.func = INSTR_MERGED_PUSH_N;
                i0.word = (uint8_t)t;
            }
            continue;
        }

        if(i0.func == INSTR_POP)
        {
            uint32_t t;
            for(t = 1; t < MAX_INSTR_CYCLES / 2; ++t)
            {
                if(n + t >= merged_prog.size()) break;
                if(merged_prog[n + t].func != INSTR_POP) break;
            }
            if(t > 1)
            {
                i0.func = INSTR_MERGED_POP_N;
                i0.word = (uint8_t)t;
            }
            continue;
        }

        if(i0.func == INSTR_LDI)
        {
            uint32_t t;
            for(t = 1; t < MAX_INSTR_CYCLES; ++t)
            {
                if(n + t >= merged_prog.size()) break;
                if(merged_prog[n + t].func != INSTR_LDI) break;
            }
            if(t > 1)
            {
                i0.func = INSTR_MERGED_LDI_N;
                i0.word = (uint8_t)t;
            }
            continue;
        }

        if(i0.func == INSTR_DEC)
        {
            auto const& ti = merged_prog[n + 1];
            if(ti.func == INSTR_BRBC && ti.src == 1)
                i0.func = INSTR_MERGED_DEC_BRNE;
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

        if(i0.func == INSTR_SUBI &&
            i1.func == INSTR_SBCI)
        {
            i0.func = INSTR_MERGED_SUBI_SBCI;
            i0.word = i0.src + i1.src * 256;
            i0.src = i1.dst;
            continue;
        }
    }
}

}
