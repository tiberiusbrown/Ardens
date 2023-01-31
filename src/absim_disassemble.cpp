#include "absim.hpp"

namespace absim
{

static uint8_t bit_from_mask(uint8_t m)
{
    uint8_t r = 0;
    while(!(m & 1))
        ++r, m >>= 1;
    return r;
}

static char const* get_instr_name(avr_instr_t const& i)
{
    switch(i.func)
    {
        case INSTR_RCALL   : return "rcall";
        case INSTR_CALL    : return "call";
        case INSTR_ICALL   : return "icall";
        case INSTR_RET     : return "ret";
        case INSTR_RETI    : return "reti";
        case INSTR_MOVW    : return "movw";
        case INSTR_MOV     : return "mov";
        case INSTR_AND     : return "and";
        case INSTR_OR      : return "or";
        case INSTR_EOR     : return "eor";
        case INSTR_ADD     : return "add";
        case INSTR_ADC     : return "adc";
        case INSTR_SUB     : return "sub";
        case INSTR_SBC     : return "sbc";
        case INSTR_CPI     : return "cpi";
        case INSTR_CP      : return "cp";
        case INSTR_CPC     : return "cpc";
        case INSTR_OUT     : return "out";
        case INSTR_IN      : return "in";
        case INSTR_LDI     : return "ldi";
        case INSTR_LPM     : return "lpm";
        case INSTR_BRBS    : return "brbs";
        case INSTR_BRBC    : return "brbc";
        case INSTR_LDS     : return "lds";
        case INSTR_STS     : return "sts";
        case INSTR_CPSE    : return "cpse";
        case INSTR_SUBI    : return "subi";
        case INSTR_SBCI    : return "sbci";
        case INSTR_ORI     : return "ori";
        case INSTR_ANDI    : return "andi";
        case INSTR_ADIW    : return "adiw";
        case INSTR_SBIW    : return "sbiw";
        case INSTR_SBI     : return "sbi";
        case INSTR_CBI     : return "cbi";
        case INSTR_SBIS    : return "sbis";
        case INSTR_SBIC    : return "sbic";
        case INSTR_SBRS    : return "sbrs";
        case INSTR_SBRC    : return "sbrc";
        case INSTR_BLD     : return "bld";
        case INSTR_BST     : return "bst";
        case INSTR_COM     : return "com";
        case INSTR_NEG     : return "neg";
        case INSTR_SWAP    : return "swap";
        case INSTR_INC     : return "inc";
        case INSTR_DEC     : return "dec";
        case INSTR_ASR     : return "asr";
        case INSTR_LSR     : return "lsr";
        case INSTR_ROR     : return "ror";
        case INSTR_SLEEP   : return "sleep";
        case INSTR_MUL     : return "mul";
        case INSTR_MULS    : return "muls";
        case INSTR_MULSU   : return "mulsu";
        case INSTR_FMUL    : return "fmul";
        case INSTR_FMULS   : return "fmuls";
        case INSTR_FMULSU  : return "fmulsu";
        case INSTR_NOP     : return "nop";
        case INSTR_RJMP    : return "rjmp";
        case INSTR_JMP     : return "jmp";
        case INSTR_IJMP    : return "ijmp";
        case INSTR_BSET    :
        {
            static char const* const NAMES[] =
            {
                "sec", "sez", "sen", "sev", "ses", "seh" ,"set", "sei"
            };
            return NAMES[i.src & 0x7];
        }
        case INSTR_BCLR    :
        {
            static char const* const NAMES[] =
            {
                "clc", "clz", "cln", "clv", "cls", "clh" ,"clt", "cli"
            };
            return NAMES[i.src & 0x7];
        }
        case INSTR_LDD_STD :
            return (i.word & 0x200) ? "std" : "ldd";
        case INSTR_LD_ST   :
            return i.word ? "st" : "ld";
        case INSTR_PUSH_POP:
            return i.word ? "push" : "pop";
        case INSTR_WDR:
            return "wdr";
        default: return nullptr;
    }
}

void disassemble_instr(avr_instr_t const& i, disassembled_instr_t& d)
{
    memset(&d, 0, sizeof(disassembled_instr_t));
    d.name = get_instr_name(i);
    d.type = disassembled_instr_t::INSTR;

    switch(i.func)
    {
    case INSTR_CPC:
    case INSTR_SBC:
    case INSTR_ADD:
    case INSTR_CPSE:
    case INSTR_CP:
    case INSTR_SUB:
    case INSTR_ADC:
    case INSTR_AND:
    case INSTR_EOR:
    case INSTR_OR:
    case INSTR_MOV:
    case INSTR_MUL:
    case INSTR_MULS:
    case INSTR_MULSU:
    case INSTR_FMUL:
    case INSTR_FMULS:
    case INSTR_FMULSU:
        d.arg0.type = disassembled_instr_arg_t::type::REG;
        d.arg1.type = disassembled_instr_arg_t::type::REG;
        d.arg0.val = i.dst;
        d.arg1.val = i.src;
        break;
    case INSTR_MOVW:
        d.arg0.type = disassembled_instr_arg_t::type::REG;
        d.arg1.type = disassembled_instr_arg_t::type::REG;
        d.arg0.val = i.dst * 2;
        d.arg1.val = i.src * 2;
        break;
    case INSTR_LDI:
    case INSTR_CPI:
    case INSTR_ADIW:
    case INSTR_SBIW:
    case INSTR_ANDI:
    case INSTR_ORI:
    case INSTR_SUBI:
    case INSTR_SBCI:
        d.arg0.type = disassembled_instr_arg_t::type::REG;
        d.arg1.type = disassembled_instr_arg_t::type::IMM;
        d.arg0.val = i.dst;
        d.arg1.val = i.src;
        break;
    case INSTR_COM:
    case INSTR_NEG:
    case INSTR_SWAP:
    case INSTR_INC:
    case INSTR_ASR:
    case INSTR_LSR:
    case INSTR_ROR:
    case INSTR_DEC:
        d.arg0.type = disassembled_instr_arg_t::type::REG;
        d.arg0.val = i.dst;
        break;
    case INSTR_PUSH_POP:
        d.arg0.type = disassembled_instr_arg_t::type::REG;
        d.arg0.val = i.src;
        break;
    case INSTR_LDS:
        d.arg0.type = disassembled_instr_arg_t::type::REG;
        d.arg1.type = disassembled_instr_arg_t::type::DS_ADDR;
        d.arg0.val = i.dst;
        d.arg1.val = i.word;
        break;
    case INSTR_STS:
        d.arg0.type = disassembled_instr_arg_t::type::DS_ADDR;
        d.arg1.type = disassembled_instr_arg_t::type::REG;
        d.arg0.val = i.word;
        d.arg1.val = i.src;
        break;
    case INSTR_BLD:
    case INSTR_BST:
    case INSTR_SBRS:
    case INSTR_SBRC:
        d.arg0.type = disassembled_instr_arg_t::type::REG;
        d.arg1.type = disassembled_instr_arg_t::type::BIT;
        d.arg0.val = i.dst;
        d.arg1.val = bit_from_mask(i.src);
        break;

    case INSTR_CALL:
    case INSTR_JMP:
        d.arg0.type = disassembled_instr_arg_t::type::PROG_ADDR;
        d.arg0.val = i.word;
        break;

    case INSTR_RJMP:
    case INSTR_RCALL:
    case INSTR_BRBC:
    case INSTR_BRBS:
        d.arg0.type = disassembled_instr_arg_t::type::OFFSET;
        d.arg0.val = i.word;
        break;

    case INSTR_LDD_STD:
    {
        auto reg_type = disassembled_instr_arg_t::type::PTR_REG_OFFSET;
        if(i.dst == 0)
            reg_type = disassembled_instr_arg_t::type::PTR_REG;
        uint8_t reg = (i.word & 0x8) ? 28 : 30;
        if(i.word & 0x200)
        {
            // std
            if(i.dst == 0) d.name = "st";
            d.arg0.type = reg_type;
            d.arg1.type = disassembled_instr_arg_t::type::REG;
            d.arg0.val = reg | (uint16_t(i.dst) << 8);
            d.arg1.val = i.src;
        }
        else
        {
            // ldd
            if(i.dst == 0) d.name = "ld";
            d.arg0.type = disassembled_instr_arg_t::type::REG;
            d.arg1.type = reg_type;
            d.arg0.val = i.src;
            d.arg1.val = reg | (uint16_t(i.dst) << 8);
        }
        break;
    }

    case INSTR_LD_ST:
    {
        uint8_t reg;
        if(i.dst <= 2) reg = 30;
        else if(i.dst <= 10) reg = 28;
        else reg = 26;

        auto reg_type = disassembled_instr_arg_t::type::PTR_REG;
        if(i.dst & 0x2)
            reg_type = disassembled_instr_arg_t::type::PTR_REG_PRE_DEC;
        if(i.dst & 0x1)
            reg_type = disassembled_instr_arg_t::type::PTR_REG_POST_INC;

        // st
        d.arg0.type = reg_type;
        d.arg1.type = disassembled_instr_arg_t::type::REG;
        d.arg0.val = reg;
        d.arg1.val = i.src;

        // ld
        if(i.word)
            std::swap(d.arg0, d.arg1);

        break;
    }

    case INSTR_LPM:
        if(i.word < 2)
        {
            auto reg_type = disassembled_instr_arg_t::type::PTR_REG;
            if(i.word == 1)
                reg_type = disassembled_instr_arg_t::type::PTR_REG_POST_INC;
            d.arg0.type = disassembled_instr_arg_t::type::REG;
            d.arg1.type = reg_type;
            d.arg0.val = i.dst;
            d.arg1.val = 30;
        }
        break;

    case INSTR_OUT:
        d.arg0.type = disassembled_instr_arg_t::type::IO_REG;
        d.arg1.type = disassembled_instr_arg_t::type::REG;
        d.arg0.val = i.dst;
        d.arg1.val = i.src;
        break;
    case INSTR_IN:
        d.arg0.type = disassembled_instr_arg_t::type::REG;
        d.arg1.type = disassembled_instr_arg_t::type::IO_REG;
        d.arg0.val = i.dst;
        d.arg1.val = i.src;
        break;

    case INSTR_SBI:
    case INSTR_CBI:
    case INSTR_SBIS:
    case INSTR_SBIC:
        d.arg0.type = disassembled_instr_arg_t::type::IO_REG;
        d.arg1.type = disassembled_instr_arg_t::type::BIT;
        d.arg0.val = i.dst;
        d.arg1.val = bit_from_mask(i.src);
        break;


    default:
        break;
    }

    // aliases
    if(i.src == i.dst)
    {
        if(i.func == INSTR_ADD)
        {
            d.name = "lsl";
            d.arg1.type = disassembled_instr_arg_t::type::NONE;
        }
        if(i.func == INSTR_ADC)
        {
            d.name = "rol";
            d.arg1.type = disassembled_instr_arg_t::type::NONE;
        }
        if(i.func == INSTR_EOR)
        {
            d.name = "clr";
            d.arg1.type = disassembled_instr_arg_t::type::NONE;
        }
        if(i.func == INSTR_AND)
        {
            d.name = "tst";
            d.arg1.type = disassembled_instr_arg_t::type::NONE;
        }
    }
    if(i.func == INSTR_BRBC)
    {
        switch(i.src)
        {
        case 0: d.name = "brcc"; break;
        case 1: d.name = "brne"; break;
        case 2: d.name = "brpl"; break;
        case 3: d.name = "brvc"; break;
        case 4: d.name = "brge"; break;
        case 5: d.name = "brhc"; break;
        case 6: d.name = "brtc"; break;
        case 7: d.name = "brid"; break;
        default: break;
        }
    }
    if(i.func == INSTR_BRBS)
    {
        switch(i.src)
        {
        case 0: d.name = "brcs"; break;
        case 1: d.name = "breq"; break;
        case 2: d.name = "brmi"; break;
        case 3: d.name = "brvs"; break;
        case 4: d.name = "brlt"; break;
        case 5: d.name = "brhs"; break;
        case 6: d.name = "brts"; break;
        case 7: d.name = "brie"; break;
        default: break;
        }
    }
}

}
