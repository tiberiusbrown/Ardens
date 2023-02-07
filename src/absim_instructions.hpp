#pragma once

namespace absim
{

struct atmega32u4_t;
struct avr_instr_t;

using instr_func_t = uint32_t(*)(atmega32u4_t& cpu, avr_instr_t const& i);

extern instr_func_t const INSTR_MAP[];
extern instr_func_t const INSTR_MAP_MERGED[];

struct disassembled_instr_arg_t
{
    enum class type : uint8_t
    {
        NONE,
        REG,
        PTR_REG,
        PTR_REG_PRE_DEC,
        PTR_REG_POST_INC,
        PTR_REG_OFFSET,
        IO_REG,
        IMM,
        BIT,
        PROG_ADDR,
        DS_ADDR,
        OFFSET,
    } type;
    uint16_t val;
};
struct disassembled_instr_t
{
    char const* name;
    uint16_t addr;
    enum { INSTR, OBJECT, SOURCE, SYMBOL} type;
    disassembled_instr_arg_t arg0;
    disassembled_instr_arg_t arg1;
};
void disassemble_instr(avr_instr_t const& i, disassembled_instr_t& d);
bool instr_is_two_words(avr_instr_t const& i);

enum
{
    INSTR_UNKNOWN,
    INSTR_RCALL,
    INSTR_CALL,
    INSTR_ICALL,
    INSTR_RET,
    INSTR_RETI,
    INSTR_MOVW,
    INSTR_MOV,
    INSTR_AND,
    INSTR_OR,
    INSTR_EOR,
    INSTR_ADD,
    INSTR_ADC,
    INSTR_SUB,
    INSTR_SBC,
    INSTR_CPI,
    INSTR_CP,
    INSTR_CPC,
    INSTR_OUT,
    INSTR_IN,
    INSTR_LDI,
    INSTR_LPM,
    INSTR_BRBS,
    INSTR_BRBC,
    INSTR_LDS,
    INSTR_STS,
    INSTR_LDD_STD,
    INSTR_LD_ST,
    INSTR_PUSH,
    INSTR_POP,
    INSTR_CPSE,
    INSTR_SUBI,
    INSTR_SBCI,
    INSTR_ORI,
    INSTR_ANDI,
    INSTR_ADIW,
    INSTR_SBIW,
    INSTR_BSET,
    INSTR_BCLR,
    INSTR_SBI,
    INSTR_CBI,
    INSTR_SBIS,
    INSTR_SBIC,
    INSTR_SBRS,
    INSTR_SBRC,
    INSTR_BLD,
    INSTR_BST,
    INSTR_COM,
    INSTR_NEG,
    INSTR_SWAP,
    INSTR_INC,
    INSTR_DEC,
    INSTR_ASR,
    INSTR_LSR,
    INSTR_ROR,
    INSTR_SLEEP,
    INSTR_MUL,
    INSTR_MULS,
    INSTR_MULSU,
    INSTR_FMUL,
    INSTR_FMULS,
    INSTR_FMULSU,
    INSTR_NOP,
    INSTR_RJMP,
    INSTR_JMP,
    INSTR_IJMP,
    INSTR_WDR,

    // merged instrs
    INSTR_MERGED_PUSH_N,
    INSTR_MERGED_POP_N,
    INSTR_MERGED_LDI_N,
    INSTR_MERGED_DEC_BRNE,
};

uint32_t instr_rcall   (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_call    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_icall   (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_ret     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_reti    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_movw    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_mov     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_and     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_or      (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_eor     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_add     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_adc     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sub     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sbc     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_cpi     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_cp      (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_cpc     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_out     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_in      (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_ldi     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_lpm     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_brbs    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_brbc    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_lds     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sts     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_ldd_std (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_ld_st   (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_push    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_pop     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_cpse    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_subi    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sbci    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_ori     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_andi    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_adiw    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sbiw    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_bset    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_bclr    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sbi     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_cbi     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sbis    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sbic    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sbrs    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sbrc    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_bld     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_bst     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_com     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_neg     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_swap    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_inc     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_dec     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_asr     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_lsr     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_ror     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_sleep   (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_mul     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_muls    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_mulsu   (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_fmul    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_fmuls   (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_fmulsu  (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_nop     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_rjmp    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_jmp     (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_ijmp    (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_wdr     (atmega32u4_t& cpu, avr_instr_t const& i);

// merged instrs
uint32_t instr_merged_push_n  (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_merged_pop_n   (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_merged_ldi_n   (atmega32u4_t& cpu, avr_instr_t const& i);
uint32_t instr_merged_dec_brne(atmega32u4_t& cpu, avr_instr_t const& i);

}
