#include "absim.hpp"

#include "absim_instructions.hpp"

namespace absim
{

static void decode_instr(avr_instr_t& i, uint16_t w0, uint16_t w1)
{
    i.func = INSTR_UNKNOWN;
    i.src = 0;
    i.dst = 0;
    i.word = 0;

    if(w0 == 0x9588) i.func = INSTR_SLEEP;
    if(w0 == 0x9508) i.func = INSTR_RET;
    if(w0 == 0x9509) i.func = INSTR_ICALL;
    if(w0 == 0x9518) i.func = INSTR_RETI;
    if(w0 == 0x9409) i.func = INSTR_IJMP;
    if(w0 == 0x95a8) i.func = INSTR_WDR;
    if(w0 == 0x0000) i.func = INSTR_NOP;
    if(i.func != INSTR_UNKNOWN) return;

    uint8_t imm8 = ((w0 >> 4) & 0xf0) | (w0 & 0xf);
    uint8_t rd_upper = ((w0 >> 4) & 0xf) + 16;

    // rjmp and rcall
    if((w0 & 0xe000) == 0xc000)
    {
        uint16_t off = w0 & 0xfff;
        if(off & 0x800) off |= 0xf000;
        i.word = off;
        i.func = (w0 & 0x1000) ? INSTR_RCALL : INSTR_RJMP;
        return;
    }

    // direct register addressing
    if((w0 & 0xc000) == 0x0000)
    {
        uint8_t src = uint8_t(((w0 >> 5) & 0x10) | (w0 & 0xf));
        uint8_t dst = uint8_t((w0 >> 4) & 0x1f);
        bool found = false;
        switch((w0 >> 10) & 0xf)
        {
        case 0x0:
            if((w0 & 0x0300) == 0x0100)
            {
                found = true;
                dst &= 0xf;
                i.func = INSTR_MOVW;
            }
            break;
        case 0x1: i.func = INSTR_CPC;  found = true; break;
        case 0x2: i.func = INSTR_SBC;  found = true; break;
        case 0x3: i.func = INSTR_ADD;  found = true; break;
        case 0x4: i.func = INSTR_CPSE; found = true; break;
        case 0x5: i.func = INSTR_CP;   found = true; break;
        case 0x6: i.func = INSTR_SUB;  found = true; break;
        case 0x7: i.func = INSTR_ADC;  found = true; break;
        case 0x8: i.func = INSTR_AND;  found = true; break;
        case 0x9: i.func = INSTR_EOR;  found = true; break;
        case 0xa: i.func = INSTR_OR;   found = true; break;
        case 0xb: i.func = INSTR_MOV;  found = true; break;
        default:
            break;
        }
        if(found)
        {
            i.src = src;
            i.dst = dst;
            return;
        }
    }

    // in/out
    if((w0 & 0xf000) == 0xb000)
    {
        uint8_t reg = (w0 >> 4) & 0x1f;
        uint8_t io = ((w0 >> 5) & 0x30) | (w0 & 0xf);
        if(w0 & 0x0800)
            i.dst = io, i.src = reg, i.func = INSTR_OUT;
        else
            i.src = io, i.dst = reg, i.func = INSTR_IN;
        return;
    }

    // ldi
    if((w0 & 0xf000) == 0xe000)
    {
        i.dst = rd_upper;
        i.src = imm8;
        i.func = INSTR_LDI;
        return;
    }

    // cpi
    if((w0 & 0xf000) == 0x3000)
    {
        i.dst = rd_upper;
        i.src = imm8;
        i.func = INSTR_CPI;
        return;
    }

    // lpm (simple)
    if(w0 == 0x95c8)
    {
        i.func = INSTR_LPM;
        i.dst = 0;
        i.word = 2; // signifies simple
        return;
    }

    // lpm
    if((w0 & 0xfe0e) == 0x9004)
    {
        i.func = INSTR_LPM;
        i.dst = (w0 >> 4) & 0x1f;
        i.word = w0 & 1; // post-increment bit
        return;
    }

    uint16_t branch = (w0 >> 3) & 0x7f;
    if(branch & 0x40) branch |= 0xff80;

    // brbs
    if((w0 & 0xfc00) == 0xf000)
    {
        i.src = uint8_t(w0 & 0x7);
        i.word = branch;
        i.func = INSTR_BRBS;
        return;
    }

    // brbc
    if((w0 & 0xfc00) == 0xf400)
    {
        i.src = uint8_t(w0 & 0x7);
        i.word = branch;
        i.func = INSTR_BRBC;
        return;
    }

    // lds
    if((w0 & 0xfe0f) == 0x9000)
    {
        i.dst = uint8_t(w0 >> 4) & 0x1f;
        i.word = w1;
        i.func = INSTR_LDS;
        return;
    }

    // sts
    if((w0 & 0xfe0f) == 0x9200)
    {
        i.src = uint8_t(w0 >> 4) & 0x1f;
        i.word = w1;
        i.func = INSTR_STS;
        return;
    }

    // ldd and std
    if((w0 & 0xd000) == 0x8000)
    {
        uint8_t reg = uint8_t((w0 >> 4) & 0x1f);
        uint8_t q = uint8_t(
            (w0 & 0x7) | ((w0 >> 7) & 0x18) | ((w0 >> 8) & 0x20));
        i.src = reg;
        i.dst = q;
        i.word = w0 & 0x0208;
        i.func = INSTR_LDD_STD;
        return;
    }

    // ld and st (with post-inc/pre-dec) and push/pop
    if((w0 & 0xfc00) == 0x9000)
    {
        uint8_t reg = uint8_t((w0 >> 4) & 0x1f);
        uint8_t n = uint8_t(w0 & 0xf);
        if(n != 0 && n != 11 && (n <= 2 || n >= 9))
        {
            i.src = reg;
            i.dst = n;
            uint16_t t = w0 & 0x0200;
            i.word = w0 & 0x0200;
            if(n == 0xf)
            {
                i.func = t ? INSTR_PUSH : INSTR_POP;
            }
            else
            {
                i.func = INSTR_LD_ST;
            }
            return;
        }
    }

    // jmp and call
    if((w0 & 0xfe0c) == 0x940c)
    {
        i.word = w1 & 0x3fff;
        i.func = (w0 & 0x2) ? INSTR_CALL : INSTR_JMP;
        return;
    }

    // adiw and sbiw
    if((w0 & 0xfe00) == 0x9600)
    {
        i.dst = 24 + ((w0 >> 3) & 0x6);
        i.src = uint8_t((w0 & 0xf) | ((w0 >> 2) & 0x30));
        i.func = (w0 & 0x0100) ? INSTR_SBIW : INSTR_ADIW;
        return;
    }

    if((w0 & 0xc000) == 0x4000)
    {
        i.src = uint8_t((w0 & 0xf) | ((w0 >> 4) & 0xf0));
        i.dst = uint8_t(16 + ((w0 >> 4) & 0xf));
        uint8_t n = uint8_t((w0 >> 12) & 0x3);
        if(n == 0) i.func = INSTR_SBCI;
        if(n == 1) i.func = INSTR_SUBI;
        if(n == 2) i.func = INSTR_ORI;
        if(n == 3) i.func = INSTR_ANDI;
        return;
    }

    if((w0 & 0xff0f) == 0x9408)
    {
        i.src = uint8_t((w0 >> 4) & 0x7);
        i.dst = (1 << i.src);
        i.func = INSTR_BSET;
        if(w0 & 0x80)
        {
            i.func = INSTR_BCLR;
            i.dst = ~i.dst;
        }
        i.func = (w0 & 0x80) ? INSTR_BCLR : INSTR_BSET;
        return;
    }

    if((w0 & 0xfc00) == 0x9800)
    {
        uint8_t n = uint8_t((w0 >> 8) & 0x3);
        i.dst = uint8_t((w0 >> 3) & 0x1f);
        i.src = 1 << uint8_t(w0 & 0x7);
        if(n == 0) i.func = INSTR_CBI;
        if(n == 1) i.func = INSTR_SBIC;
        if(n == 2) i.func = INSTR_SBI;
        if(n == 3) i.func = INSTR_SBIS;
        return;
    }

    if((w0 & 0xf808) == 0xf800)
    {
        uint8_t n = uint8_t((w0 >> 9) & 0x3);
        i.dst = uint8_t((w0 >> 4) & 0x1f);
        i.src = 1 << uint8_t(w0 & 0x7);
        if(n == 0) i.func = INSTR_BLD;
        if(n == 1) i.func = INSTR_BST;
        if(n == 2) i.func = INSTR_SBRC;
        if(n == 3) i.func = INSTR_SBRS;
        return;
    }

    if((w0 & 0xfe00) == 0x9400)
    {
        bool found = false;
        switch(w0 & 0xf)
        {
        case 0x0: i.func = INSTR_COM;  found = true; break;
        case 0x1: i.func = INSTR_NEG;  found = true; break;
        case 0x2: i.func = INSTR_SWAP; found = true; break;
        case 0x3: i.func = INSTR_INC;  found = true; break;
        case 0x5: i.func = INSTR_ASR;  found = true; break;
        case 0x6: i.func = INSTR_LSR;  found = true; break;
        case 0x7: i.func = INSTR_ROR;  found = true; break;
        case 0xa: i.func = INSTR_DEC;  found = true; break;
        default: break;
        }
        if(found)
        {
            i.dst = uint8_t((w0 >> 4) & 0x1f);
            return;
        }
    }

    if((w0 & 0xfc00) == 0x9c00)
    {
        i.dst = uint8_t((w0 >> 4) & 0x1f);
        i.src = uint8_t((w0 & 0xf) | ((w0 >> 5) & 0x10));
        i.func = INSTR_MUL;
        return;
    }

    if((w0 & 0xff00) == 0x0200)
    {
        i.dst = uint8_t(16 + ((w0 >> 4) & 0xf));
        i.src = uint8_t(16 + ((w0 >> 0) & 0xf));
        i.func = INSTR_MULS;
        return;
    }

    if((w0 & 0xff00) == 0x0300)
    {
        uint8_t n = ((w0 >> 3) & 0x1) | ((w0 >> 6) & 0x2);
        i.dst = uint8_t(16 + ((w0 >> 4) & 0x7));
        i.src = uint8_t(16 + ((w0 >> 0) & 0x7));
        if(n == 0) i.func = INSTR_MULSU;
        if(n == 1) i.func = INSTR_FMUL;
        if(n == 2) i.func = INSTR_FMULS;
        if(n == 3) i.func = INSTR_FMULSU;
        return;
    }
}

void atmega32u4_t::decode()
{
    uint16_t w0, w1, lo, hi;
    for(int i = 0; i < PROG_SIZE_BYTES / 2; ++i)
    {
        lo = prog[i * 2 + 0];
        hi = prog[i * 2 + 1];
        w0 = lo | (hi << 8);
        w1 = 0;
        if(i + 1 < PROG_SIZE_BYTES / 2)
        {
            lo = prog[i * 2 + 2];
            hi = prog[i * 2 + 3];
            w1 = lo | (hi << 8);
        }
        decode_instr(decoded_prog[i], w0, w1);
    }

    // disassemble
    num_instrs = 0;
    uint16_t addr = 0;
    while(addr + 1 < last_addr)
    {
        auto const& i = decoded_prog[addr / 2];
        auto& d = disassembled_prog[num_instrs];

        disassemble_instr(i, d);
        d.addr = addr;

        ++num_instrs;
        addr += instr_is_two_words(i) ? 4 : 2;
    }

    merge_instrs();

    decoded = true;
}

}
