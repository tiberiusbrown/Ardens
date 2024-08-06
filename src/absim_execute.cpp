#include "absim.hpp"

#include "absim_instructions.hpp"

// TODO: clean up remaining flags logic

namespace absim
{

instr_func_t const INSTR_MAP[NUM_INSTR] =
{
    instr_unknown,
    instr_rcall,
    instr_call,
    instr_icall,
    instr_ret,
    instr_reti,
    instr_movw,
    instr_mov,
    instr_and,
    instr_or,
    instr_eor,
    instr_clr,
    instr_add,
    instr_adc,
    instr_sub,
    instr_sbc,
    instr_cpi,
    instr_cp,
    instr_cpc,
    instr_out,
    instr_in,
    instr_ldi,
    instr_lpm,
    instr_brbs,
    instr_brbc,
    instr_lds,
    instr_sts,
    instr_ldd_y,
    instr_ldd_z,
    instr_std_y,
    instr_std_z,
    instr_ld_st,
    instr_ld_x,
    instr_ld_y,
    instr_ld_z,
    instr_ld_x_inc,
    instr_ld_y_inc,
    instr_ld_z_inc,
    instr_ld_x_dec,
    instr_ld_y_dec,
    instr_ld_z_dec,
    instr_st_x,
    instr_st_y,
    instr_st_z,
    instr_st_x_inc,
    instr_st_y_inc,
    instr_st_z_inc,
    instr_st_x_dec,
    instr_st_y_dec,
    instr_st_z_dec,
    instr_push,
    instr_pop,
    instr_cpse,
    instr_subi,
    instr_sbci,
    instr_ori,
    instr_andi,
    instr_adiw,
    instr_sbiw,
    instr_bset,
    instr_bclr,
    instr_sbi,
    instr_cbi,
    instr_sbis,
    instr_sbic,
    instr_sbrs,
    instr_sbrc,
    instr_bld,
    instr_bst,
    instr_com,
    instr_neg,
    instr_swap,
    instr_inc,
    instr_dec,
    instr_asr,
    instr_lsr,
    instr_ror,
    instr_sleep,
    instr_mul,
    instr_muls,
    instr_mulsu,
    instr_fmul,
    instr_fmuls,
    instr_fmulsu,
    instr_nop,
    instr_rjmp,
    instr_jmp,
    instr_ijmp,
    instr_wdr,
    instr_spm,
    instr_break,

    // merged instrs

    instr_merged_out,
    instr_merged_in,
    instr_merged_lds,
    instr_merged_sts,
    instr_merged_ldd_y,
    instr_merged_ldd_z,
    instr_merged_std_y,
    instr_merged_std_z,
    instr_merged_ld_st,
    instr_merged_ld_x,
    instr_merged_ld_y,
    instr_merged_ld_z,
    instr_merged_ld_x_inc,
    instr_merged_ld_y_inc,
    instr_merged_ld_z_inc,
    instr_merged_ld_x_dec,
    instr_merged_ld_y_dec,
    instr_merged_ld_z_dec,
    instr_merged_st_x,
    instr_merged_st_y,
    instr_merged_st_z,
    instr_merged_st_x_inc,
    instr_merged_st_y_inc,
    instr_merged_st_z_inc,
    instr_merged_st_x_dec,
    instr_merged_st_y_dec,
    instr_merged_st_z_dec,
    instr_merged_sbi,
    instr_merged_cbi,
    instr_merged_sbis,
    instr_merged_sbic,

    instr_merged_ldi2,
    instr_merged_dec_brne,
    instr_merged_add_adc,
    instr_merged_sub_sbc,
    instr_merged_cp_cpc,
    instr_merged_subi_sbci,
    instr_merged_delay,
};

bool instr_is_two_words(avr_instr_t i)
{
    return
        i.func == INSTR_CALL ||
        i.func == INSTR_JMP ||
        i.func == INSTR_LDS ||
        i.func == INSTR_STS;
}

bool instr_is_call(avr_instr_t i)
{
    return
        i.func == INSTR_CALL ||
        i.func == INSTR_RCALL ||
        i.func == INSTR_ICALL;
}

bool instr_is_ret(avr_instr_t i)
{
    return
        i.func == INSTR_RET ||
        i.func == INSTR_RETI;
}

ARDENS_FORCEINLINE static bool next_instr_is_two_words(atmega32u4_t const& cpu)
{
    if(cpu.pc + 1 >= cpu.decoded_prog.size())
        return false;
    return instr_is_two_words(cpu.decoded_prog[cpu.pc + 1]);
}

uint32_t instr_nop(atmega32u4_t& cpu, avr_instr_t i)
{
    (void)i;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_unknown(atmega32u4_t& cpu, avr_instr_t i)
{
    (void)cpu;
    (void)i;
    return 1;
}

uint32_t instr_wdr(atmega32u4_t& cpu, avr_instr_t i)
{
    (void)i;
    cpu.watchdog_divider_cycle = 0;
    cpu.watchdog_prev_cycle = cpu.cycle_count;
    cpu.watchdog_next_cycle = cpu.cycle_count + cpu.watchdog_divider;
    cpu.peripheral_queue.schedule(cpu.watchdog_next_cycle, PQ_WATCHDOG);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_spm(atmega32u4_t& cpu, avr_instr_t i)
{
    (void)i;
    cpu.execute_spm();
    cpu.pc += 1;
    return 1;
}

uint32_t instr_break(atmega32u4_t& cpu, avr_instr_t i)
{
    (void)i;
    cpu.pc += 1;
    cpu.autobreak(AB_BREAK);
    return 1;
}

ARDENS_FORCEINLINE static void set_flag(atmega32u4_t& cpu, uint8_t mask, uint32_t x)
{
    if(x) cpu.sreg() |= mask;
    else cpu.sreg() &= ~mask;
}

ARDENS_FORCEINLINE static uint8_t flag_s(uint8_t sreg)
{
    // S |= N ^ V
    sreg |= (((sreg ^ (sreg >> 1)) & 0x4) << 2);
    return sreg;
}

ARDENS_FORCEINLINE static void set_flags_hcv(atmega32u4_t& cpu, uint8_t h, uint8_t c, uint8_t v)
{
    uint8_t sreg = cpu.sreg() & ~(SREG_H | SREG_C | SREG_V);

    if(h) sreg |= SREG_H;
    if(c) sreg |= SREG_C;
    if(v) sreg |= SREG_V;

    cpu.sreg() = sreg;
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

ARDENS_FORCEINLINE static void set_flags_nzs(atmega32u4_t& cpu, uint32_t x)
{
    uint8_t sreg = cpu.sreg() & ~(SREG_N | SREG_Z | SREG_S);
    sreg = flags_nzs(sreg, x);
    cpu.sreg() = sreg;
}

uint32_t instr_rjmp(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.pc += (int16_t)i.word + 1;
    return 2;
}

uint32_t instr_jmp(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.pc = i.word;
    return 3;
}

uint32_t instr_ijmp(atmega32u4_t& cpu, avr_instr_t i)
{
    (void)i;
    uint16_t z = cpu.z_word();
    if(z >= cpu.last_addr / 2)
    {
        cpu.autobreak(AB_OOB_IJMP);
        if(cpu.enabled_autobreaks.test(AB_OOB_IJMP))
        {
            // cancel instruction so user has a chance for introspection
            return 2;
        }
    }
    cpu.pc = z;
    return 2;
}

uint32_t instr_rcall(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t ret_addr = cpu.pc + 1;
    cpu.pc += (int16_t)i.word + 1;
    if(i.word != 0)
        cpu.push_stack_frame(ret_addr);
    cpu.push(uint8_t(ret_addr >> 0));
    cpu.push(uint8_t(ret_addr >> 8));
    return 3;
}

uint32_t instr_call(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t ret_addr = cpu.pc + 2;
    cpu.pc = i.word;
    cpu.push_stack_frame(ret_addr);
    cpu.push(uint8_t(ret_addr >> 0));
    cpu.push(uint8_t(ret_addr >> 8));
    return 4;
}

uint32_t instr_icall(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t ret_addr = cpu.pc + 1;
    cpu.pc = cpu.z_word();
    cpu.push_stack_frame(ret_addr);
    cpu.push(uint8_t(ret_addr >> 0));
    cpu.push(uint8_t(ret_addr >> 8));
    return 3;
}

uint32_t instr_ret(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t hi = cpu.pop();
    uint16_t lo = cpu.pop();
    cpu.pc = lo | (hi << 8);
    cpu.pop_stack_frame();
    return 4;
}

uint32_t instr_reti(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t hi = cpu.pop();
    uint16_t lo = cpu.pop();
    cpu.pc = lo | (hi << 8);
    cpu.sreg() |= SREG_I;
    cpu.just_written = 0x5f;
    cpu.pop_stack_frame();
    return 4;
}

uint32_t instr_movw(atmega32u4_t& cpu, avr_instr_t i)
{
#if 1
    // endianness doesn't matter here
    uint16_t* dst = reinterpret_cast<uint16_t*>(&cpu.data[0]) + i.dst;
    uint16_t* src = reinterpret_cast<uint16_t*>(&cpu.data[0]) + i.src;
    *dst = *src;
#else
    cpu.gpr(i.dst * 2 + 0) = cpu.gpr(i.src * 2 + 0);
    cpu.gpr(i.dst * 2 + 1) = cpu.gpr(i.src * 2 + 1);
#endif
    cpu.pc += 1;
    return 1;
}

uint32_t instr_mov(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.gpr(i.dst) = cpu.gpr(i.src);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_and(atmega32u4_t& cpu, avr_instr_t i)
{
    uint32_t dst = cpu.gpr(i.dst);
    uint32_t res = dst & cpu.gpr(i.src);
    cpu.gpr(i.dst) = res;
    uint8_t sreg = cpu.sreg() & ~(SREG_V | SREG_N | SREG_Z | SREG_S);
    if(res == 0) sreg |= SREG_Z;
    if(res & 0x80) sreg |= (SREG_N | SREG_S);
    cpu.sreg() = sreg;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_or(atmega32u4_t& cpu, avr_instr_t i)
{
    uint32_t dst = cpu.gpr(i.dst);
    uint32_t res = dst | cpu.gpr(i.src);
    cpu.gpr(i.dst) = res;
    uint8_t sreg = cpu.sreg() & ~(SREG_V | SREG_N | SREG_Z | SREG_S);
    sreg = flags_nzs(sreg, res);
    cpu.sreg() = sreg;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_eor(atmega32u4_t& cpu, avr_instr_t i)
{
    uint32_t dst = cpu.gpr(i.dst);
    uint32_t res = dst ^ cpu.gpr(i.src);
    cpu.gpr(i.dst) = res;
    uint8_t sreg = cpu.sreg() & ~(SREG_V | SREG_N | SREG_Z | SREG_S);
    if(res == 0) sreg |= SREG_Z;
    if(res & 0x80) sreg |= (SREG_N | SREG_S);
    cpu.sreg() = sreg;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_clr(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.gpr(i.dst) = 0;
    uint8_t sreg = cpu.sreg();
    sreg &= ~0x1c;
    sreg |= 0x02;
    cpu.sreg() = sreg;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_add(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned dst = cpu.gpr(i.dst);
    unsigned src = cpu.gpr(i.src);
    unsigned res = (dst + src) & 0xff;
    cpu.gpr(i.dst) = (uint8_t)res;

    unsigned hc = (dst & src) | (src & ~res) | (~res & dst);
    unsigned v = (dst & src & ~res) | (~dst & ~src & res);
    unsigned sreg = cpu.sreg() & ~SREG_HSVNZC;
    sreg |= (hc & 0x08) << 2;    // H flag
    sreg |= hc >> 7;             // C flag
    sreg |= (v & 0x80) >> 4;     // V flag
    sreg = flags_nzs(sreg, res);
    cpu.sreg() = (uint8_t)sreg;

    cpu.pc += 1;
    return 1;
}

uint32_t instr_adc(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned dst = cpu.gpr(i.dst);
    unsigned src = cpu.gpr(i.src);
    unsigned c = cpu.sreg() & 1;
    unsigned res = (dst + src + c) & 0xff;
    cpu.gpr(i.dst) = (uint8_t)res;

    unsigned hc = (dst & src) | (src & ~res) | (~res & dst);
    unsigned v = (dst & src & ~res) | (~dst & ~src & res);
    unsigned sreg = cpu.sreg() & ~SREG_HSVNZC;
    sreg |= (hc & 0x08) << 2;    // H flag
    sreg |= hc >> 7;             // C flag
    sreg |= (v & 0x80) >> 4;     // V flag
    sreg = flags_nzs(sreg, res);
    cpu.sreg() = (uint8_t)sreg;

    cpu.pc += 1;
    return 1;
}

ARDENS_FORCEINLINE static void sub_flags(atmega32u4_t& cpu, unsigned res, unsigned dst, unsigned src)
{
    unsigned sreg = cpu.sreg() & ~SREG_HSVNZC;
    unsigned hc = (~dst & src) | (src & res) | (res & ~dst);
    unsigned v = (dst & ~src & ~res) | (~dst & src & res);
    sreg |= (hc & 0x08) << 2;    // H flag
    sreg |= hc >> 7;             // C flag
    sreg |= (v & 0x80) >> 4;     // V flag
    sreg = flags_nzs(sreg, res);
    cpu.sreg() = (uint8_t)sreg;
}

ARDENS_FORCEINLINE static void sub_imm(atmega32u4_t& cpu, unsigned rdst, unsigned imm, unsigned c)
{
    unsigned dst = cpu.gpr(rdst);
    unsigned src = imm;
    unsigned res = (dst - src - c) & 0xff;
    cpu.gpr(rdst) = (uint8_t)res;
    sub_flags(cpu, res, dst, src);
}

uint32_t instr_sub(atmega32u4_t& cpu, avr_instr_t i)
{
    sub_imm(cpu, i.dst, cpu.gpr(i.src), 0);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_sbc(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned sreg = cpu.sreg();
    unsigned dst = cpu.gpr(i.dst);
    unsigned src = cpu.gpr(i.src);
    unsigned res = (dst - src - (sreg & SREG_C)) & 0xff;
    cpu.gpr(i.dst) = (uint8_t)res;

    unsigned hc = (~dst & src) | (src & res) | (res & ~dst);
    unsigned v = (dst & ~src & ~res) | (~dst & src & res);
    unsigned z = ~sreg & SREG_Z;
    sreg &= ~SREG_HSVNZC;
    sreg |= (hc & 0x08) << 2;    // H flag
    sreg |= hc >> 7;             // C flag
    sreg |= (v & 0x80) >> 4;     // V flag
    res |= z;
    sreg = flags_nzs(sreg, res);
    cpu.sreg() = (uint8_t)sreg;

    cpu.pc += 1;
    return 1;
}

uint32_t instr_cpi(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned dst = cpu.gpr(i.dst);
    unsigned src = i.src;
    unsigned res = (dst - src) & 0xff;
    sub_flags(cpu, res, dst, src);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_cp(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned dst = cpu.gpr(i.dst);
    unsigned src = cpu.gpr(i.src);
    unsigned res = (dst - src) & 0xff;
    sub_flags(cpu, res, dst, src);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_cpc(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned sreg = cpu.sreg();
    unsigned dst = cpu.gpr(i.dst);
    unsigned src = cpu.gpr(i.src);
    unsigned res = (dst - src - (sreg & SREG_C)) & 0xff;

    unsigned hc = (~dst & src) | (src & res) | (res & ~dst);
    unsigned v = (dst & ~src & ~res) | (~dst & src & res);
    unsigned z = ~sreg & SREG_Z;
    sreg &= ~SREG_HSVNZC;
    sreg |= (hc & 0x08) << 2;    // H flag
    sreg |= hc >> 7;             // C flag
    sreg |= (v & 0x80) >> 4;     // V flag
    res |= z;
    sreg = flags_nzs(sreg, res);
    cpu.sreg() = (uint8_t)sreg;

    cpu.pc += 1;
    return 1;

}

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_out(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.st_ior<merged>(i.dst, cpu.gpr(i.src));
    cpu.pc += 1;
    return 1;
}
uint32_t instr_out(atmega32u4_t& cpu, avr_instr_t i) { return instr_out<false>(cpu, i); }
uint32_t instr_merged_out(atmega32u4_t& cpu, avr_instr_t i) { return instr_out<true>(cpu, i); }

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_in(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.gpr(i.dst) = cpu.ld_ior<merged>(i.src);
    cpu.pc += 1;
    return 1;
}
uint32_t instr_in(atmega32u4_t& cpu, avr_instr_t i) { return instr_in<false>(cpu, i); }
uint32_t instr_merged_in(atmega32u4_t& cpu, avr_instr_t i) { return instr_in<true>(cpu, i); }

uint32_t instr_ldi(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.gpr(i.dst) = i.src;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_lpm(atmega32u4_t& cpu, avr_instr_t i)
{
    // TODO: handle RWW errors (0x0000 - 0x37ff while RWWSB is set in SPMCSR)
    uint16_t z = cpu.z_word();
    uint8_t res = 0x00;
    if(z < cpu.prog.size())
        res = cpu.prog[z];
    if(cpu.spm_en_cycles != 0)
    {
        if(cpu.spm_op == cpu.SPM_OP_BLB_SET)
        {
            // reading fuse or lock bits
            switch(z)
            {
            case 0x0000: res = cpu.fuse_lo; break;
            case 0x0001: res = cpu.lock; break;
            case 0x0002: res = cpu.fuse_ext; break;
            case 0x0003: res = cpu.fuse_hi; break;
            default:
                break;
            }
        }
        else if(cpu.spm_op == cpu.SPM_OP_SIG_READ)
        {
            switch(z)
            {
            case 0x0000: res = 0x1e; break;
            case 0x0002: res = 0x95; break;
            case 0x0004: res = 0x87; break;
            case 0x0001: res = 0x6d; break;
            default:
                break;
            }
        }
    }
    cpu.gpr(i.dst) = res;
    cpu.pc += 1;
    if(i.word == 1)
    {
        // post increment z pointer
        ++z;
        cpu.gpr(30) = uint8_t(z >> 0);
        cpu.gpr(31) = uint8_t(z >> 8);
    }
    return 3;
}

uint32_t instr_brbs(atmega32u4_t& cpu, avr_instr_t i)
{
    if(cpu.sreg() & (1 << i.src))
    {
        cpu.pc += (int16_t)i.word + 1;
        return 2;
    }
    cpu.pc += 1;
    return 1;
}

uint32_t instr_brbc(atmega32u4_t& cpu, avr_instr_t i)
{
    if(!(cpu.sreg() & (1 << i.src)))
    {
        cpu.pc += (int16_t)i.word + 1;
        return 2;
    }
    cpu.pc += 1;
    return 1;
}

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_lds(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.gpr(i.dst) = cpu.ld<merged>(i.word);
    cpu.pc += 2;
    return 2;
}
uint32_t instr_lds(atmega32u4_t& cpu, avr_instr_t i) { return instr_lds<false>(cpu, i); }
uint32_t instr_merged_lds(atmega32u4_t& cpu, avr_instr_t i) { return instr_lds<true>(cpu, i); }

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_sts(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.st<merged>(i.word, cpu.gpr(i.src));
    cpu.pc += 2;
    return 2;
}
uint32_t instr_sts(atmega32u4_t& cpu, avr_instr_t i) { return instr_sts<false>(cpu, i); }
uint32_t instr_merged_sts(atmega32u4_t& cpu, avr_instr_t i) { return instr_sts<true>(cpu, i); }

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_ldd_y(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t ptr = cpu.y_word();
    if(ptr == 0)
        cpu.autobreak(AB_NULL_REL_DEREF);
    ptr += i.dst;
    cpu.gpr(i.src) = cpu.ld<merged>(ptr);
    cpu.pc += 1;
    return 2;
}
uint32_t instr_ldd_y(atmega32u4_t& cpu, avr_instr_t i) { return instr_ldd_y<false>(cpu, i); }
uint32_t instr_merged_ldd_y(atmega32u4_t& cpu, avr_instr_t i) { return instr_ldd_y<true>(cpu, i); }

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_ldd_z(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t ptr = cpu.z_word();
    if(ptr == 0)
        cpu.autobreak(AB_NULL_REL_DEREF);
    ptr += i.dst;
    cpu.gpr(i.src) = cpu.ld<merged>(ptr);
    cpu.pc += 1;
    return 2;
}
uint32_t instr_ldd_z(atmega32u4_t& cpu, avr_instr_t i) { return instr_ldd_z<false>(cpu, i); }
uint32_t instr_merged_ldd_z(atmega32u4_t& cpu, avr_instr_t i) { return instr_ldd_z<true>(cpu, i); }

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_std_y(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t ptr = cpu.y_word();
    if(ptr == 0)
        cpu.autobreak(AB_NULL_REL_DEREF);
    ptr += i.dst;
    cpu.st<merged>(ptr, cpu.gpr(i.src));
    cpu.pc += 1;
    return 2;
}
uint32_t instr_std_y(atmega32u4_t& cpu, avr_instr_t i) { return instr_std_y<false>(cpu, i); }
uint32_t instr_merged_std_y(atmega32u4_t& cpu, avr_instr_t i) { return instr_std_y<true>(cpu, i); }

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_std_z(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t ptr = cpu.z_word();
    if(ptr == 0)
        cpu.autobreak(AB_NULL_REL_DEREF);
    ptr += i.dst;
    cpu.st<merged>(ptr, cpu.gpr(i.src));
    cpu.pc += 1;
    return 2;
}
uint32_t instr_std_z(atmega32u4_t& cpu, avr_instr_t i) { return instr_std_z<false>(cpu, i); }
uint32_t instr_merged_std_z(atmega32u4_t& cpu, avr_instr_t i) { return instr_std_z<true>(cpu, i); }

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_ld_st(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t ptr;

    if(i.dst <= 2) ptr = cpu.z_word();
    else if(i.dst <= 10) ptr = cpu.y_word();
    else ptr = cpu.x_word();

    // pre-decrement
    if(i.dst & 0x2)
        --ptr;

    if(!i.word)
        cpu.gpr(i.src) = cpu.ld<merged>(ptr);
    else
        cpu.st<merged>(ptr, cpu.gpr(i.src));

    // post-increment
    if(i.dst & 0x1)
        ++ptr;

    if(i.dst & 0x3)
    {
        // store back
        if(i.dst <= 2)
        {
            cpu.gpr(30) = uint8_t(ptr >> 0);
            cpu.gpr(31) = uint8_t(ptr >> 8);
        }
        else if(i.dst <= 10)
        {
            cpu.gpr(28) = uint8_t(ptr >> 0);
            cpu.gpr(29) = uint8_t(ptr >> 8);
        }
        else
        {
            cpu.gpr(26) = uint8_t(ptr >> 0);
            cpu.gpr(27) = uint8_t(ptr >> 8);
        }
    }

    cpu.pc += 1;
    return 2;
}
uint32_t instr_ld_st(atmega32u4_t& cpu, avr_instr_t i) { return instr_ld_st<false>(cpu, i); }
uint32_t instr_merged_ld_st(atmega32u4_t& cpu, avr_instr_t i) { return instr_ld_st<true>(cpu, i); }

template<bool merged, bool ld, int reg, bool inc, bool dec>
ARDENS_FORCEINLINE static uint32_t instr_super_ld_st(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t ptr;

    if(reg == 2) ptr = cpu.z_word();
    else if(reg == 1) ptr = cpu.y_word();
    else ptr = cpu.x_word();

    // pre-decrement
    if(dec)
        --ptr;

    if(ld)
        cpu.gpr(i.src) = cpu.ld<merged>(ptr);
    else
        cpu.st<merged>(ptr, cpu.gpr(i.src));

    // post-increment
    if(inc)
        ++ptr;

    if(inc || dec)
    {
        // store back
        if(reg == 2)
        {
            cpu.gpr(30) = uint8_t(ptr >> 0);
            cpu.gpr(31) = uint8_t(ptr >> 8);
        }
        else if(reg == 1)
        {
            cpu.gpr(28) = uint8_t(ptr >> 0);
            cpu.gpr(29) = uint8_t(ptr >> 8);
        }
        else
        {
            cpu.gpr(26) = uint8_t(ptr >> 0);
            cpu.gpr(27) = uint8_t(ptr >> 8);
        }
    }

    cpu.pc += 1;
    return 2;
}

uint32_t instr_ld_x    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 1, 0, 0, 0>(cpu, i); }
uint32_t instr_ld_y    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 1, 1, 0, 0>(cpu, i); }
uint32_t instr_ld_z    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 1, 2, 0, 0>(cpu, i); }
uint32_t instr_ld_x_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 1, 0, 1, 0>(cpu, i); }
uint32_t instr_ld_y_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 1, 1, 1, 0>(cpu, i); }
uint32_t instr_ld_z_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 1, 2, 1, 0>(cpu, i); }
uint32_t instr_ld_x_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 1, 0, 0, 1>(cpu, i); }
uint32_t instr_ld_y_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 1, 1, 0, 1>(cpu, i); }
uint32_t instr_ld_z_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 1, 2, 0, 1>(cpu, i); }
uint32_t instr_st_x    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 0, 0, 0, 0>(cpu, i); }
uint32_t instr_st_y    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 0, 1, 0, 0>(cpu, i); }
uint32_t instr_st_z    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 0, 2, 0, 0>(cpu, i); }
uint32_t instr_st_x_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 0, 0, 1, 0>(cpu, i); }
uint32_t instr_st_y_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 0, 1, 1, 0>(cpu, i); }
uint32_t instr_st_z_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 0, 2, 1, 0>(cpu, i); }
uint32_t instr_st_x_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 0, 0, 0, 1>(cpu, i); }
uint32_t instr_st_y_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 0, 1, 0, 1>(cpu, i); }
uint32_t instr_st_z_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<0, 0, 2, 0, 1>(cpu, i); }

uint32_t instr_merged_ld_x    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 1, 0, 0, 0>(cpu, i); }
uint32_t instr_merged_ld_y    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 1, 1, 0, 0>(cpu, i); }
uint32_t instr_merged_ld_z    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 1, 2, 0, 0>(cpu, i); }
uint32_t instr_merged_ld_x_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 1, 0, 1, 0>(cpu, i); }
uint32_t instr_merged_ld_y_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 1, 1, 1, 0>(cpu, i); }
uint32_t instr_merged_ld_z_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 1, 2, 1, 0>(cpu, i); }
uint32_t instr_merged_ld_x_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 1, 0, 0, 1>(cpu, i); }
uint32_t instr_merged_ld_y_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 1, 1, 0, 1>(cpu, i); }
uint32_t instr_merged_ld_z_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 1, 2, 0, 1>(cpu, i); }
uint32_t instr_merged_st_x    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 0, 0, 0, 0>(cpu, i); }
uint32_t instr_merged_st_y    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 0, 1, 0, 0>(cpu, i); }
uint32_t instr_merged_st_z    (atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 0, 2, 0, 0>(cpu, i); }
uint32_t instr_merged_st_x_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 0, 0, 1, 0>(cpu, i); }
uint32_t instr_merged_st_y_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 0, 1, 1, 0>(cpu, i); }
uint32_t instr_merged_st_z_inc(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 0, 2, 1, 0>(cpu, i); }
uint32_t instr_merged_st_x_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 0, 0, 0, 1>(cpu, i); }
uint32_t instr_merged_st_y_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 0, 1, 0, 1>(cpu, i); }
uint32_t instr_merged_st_z_dec(atmega32u4_t& cpu, avr_instr_t i) { return instr_super_ld_st<1, 0, 2, 0, 1>(cpu, i); }

uint32_t instr_push(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.push(cpu.gpr(i.src));
    cpu.pc += 1;
    return 2;
}

uint32_t instr_pop(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.gpr(i.src) = cpu.pop();
    cpu.pc += 1;
    return 2;
}

uint32_t instr_cpse(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t dst = cpu.gpr(i.dst);
    uint16_t src = cpu.gpr(i.src);
    if(src != dst)
    {
        cpu.pc += 1;
        return 1;
    }
    if(next_instr_is_two_words(cpu))
    {
        cpu.pc += 3;
        return 3;
    }
    cpu.pc += 2;
    return 2;
}

uint32_t instr_subi(atmega32u4_t& cpu, avr_instr_t i)
{
    sub_imm(cpu, i.dst, i.src, 0);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_sbci(atmega32u4_t& cpu, avr_instr_t i)
{
    unsigned sreg = cpu.sreg();
    unsigned dst = cpu.gpr(i.dst);
    unsigned src = i.src;
    unsigned res = (dst - src - (sreg & SREG_C)) & 0xff;
    cpu.gpr(i.dst) = (uint8_t)res;

    unsigned hc = (~dst & src) | (src & res) | (res & ~dst);
    unsigned v = (dst & ~src & ~res) | (~dst & src & res);
    unsigned z = ~sreg & SREG_Z;
    sreg &= ~SREG_HSVNZC;
    sreg |= (hc & 0x08) << 2;    // H flag
    sreg |= hc >> 7;             // C flag
    sreg |= (v & 0x80) >> 4;     // V flag
    res |= z;
    sreg = flags_nzs(sreg, res);
    cpu.sreg() = (uint8_t)sreg;

    cpu.pc += 1;
    return 1;
}

uint32_t instr_ori(atmega32u4_t& cpu, avr_instr_t i)
{
    uint32_t dst = cpu.gpr(i.dst);
    uint32_t res = dst | i.src;
    cpu.gpr(i.dst) = res;
    uint8_t sreg = cpu.sreg() & ~(SREG_V | SREG_N | SREG_Z | SREG_S);
    if(res == 0) sreg |= SREG_Z;
    if(res & 0x80) sreg |= (SREG_N | SREG_S);
    cpu.sreg() = sreg;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_andi(atmega32u4_t& cpu, avr_instr_t i)
{
    uint32_t dst = cpu.gpr(i.dst);
    uint32_t res = dst & i.src;
    cpu.gpr(i.dst) = res;
    uint8_t sreg = cpu.sreg() & ~(SREG_V | SREG_N | SREG_Z | SREG_S);
    if(res == 0) sreg |= SREG_Z;
    if(res & 0x80) sreg |= (SREG_N | SREG_S);
    cpu.sreg() = sreg;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_adiw(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t src = i.src;
    uint16_t dst = cpu.gpr_word(i.dst);
    uint16_t res = dst + src;
    cpu.gpr(i.dst + 0) = uint8_t(res >> 0);
    cpu.gpr(i.dst + 1) = uint8_t(res >> 8);

    uint8_t sreg = cpu.sreg() & ~(SREG_Z | SREG_V | SREG_C | SREG_N | SREG_S);

    if(res == 0) sreg |= SREG_Z;         // Z
    sreg |= (~dst & res & 0x8000) >> 12; // V
    sreg |= (~res & dst & 0x8000) >> 15; // C
    sreg |= (res & 0x8000) >> 13;        // N
    sreg = flag_s(sreg);                 // S

    cpu.sreg() = sreg;

    //set_flag(cpu, SREG_Z, res == 0);
    //set_flag(cpu, SREG_V, ~dst & res & 0x8000);
    //set_flag(cpu, SREG_C, ~res & dst & 0x8000);
    //set_flag(cpu, SREG_N, res & 0x8000);
    //set_flag_s(cpu);

    cpu.pc += 1;
    return 2;
}

uint32_t instr_sbiw(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t src = i.src;
    uint16_t dst = cpu.gpr_word(i.dst);
    uint16_t res = dst - src;
    cpu.gpr(i.dst + 0) = uint8_t(res >> 0);
    cpu.gpr(i.dst + 1) = uint8_t(res >> 8);

    // in the AVR instruction set manual (seemingly a typo):
    //set_flag(cpu, SREG_V, res & ~dst & 0x8000);

    uint8_t sreg = cpu.sreg() & ~(SREG_Z | SREG_V | SREG_C | SREG_N | SREG_S);

    if(res == 0) sreg |= SREG_Z;         // Z
    sreg |= (dst & ~res & 0x8000) >> 12; // V
    sreg |= (res & ~dst & 0x8000) >> 15; // C
    sreg |= (res & 0x8000) >> 13;        // N
    sreg = flag_s(sreg);                 // S

    cpu.sreg() = sreg;

    //set_flag(cpu, SREG_Z, res == 0);
    //set_flag(cpu, SREG_V, dst & ~res & 0x8000);
    //set_flag(cpu, SREG_C, res & ~dst & 0x8000);
    //set_flag(cpu, SREG_N, res & 0x8000);
    //set_flag_s(cpu);

    cpu.pc += 1;
    return 2;
}

uint32_t instr_bset(atmega32u4_t& cpu, avr_instr_t i)
{
    // src: bit, dst: mask
    cpu.sreg() |= i.dst;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_bclr(atmega32u4_t& cpu, avr_instr_t i)
{
    // src: bit, dst: mask
    cpu.sreg() &= i.dst;
    cpu.pc += 1;
    return 1;
}

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_sbi(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.st_ior<merged>(i.dst, cpu.ld_ior<merged>(i.dst) | i.src);
    cpu.pc += 1;
    return 2;
}
uint32_t instr_sbi(atmega32u4_t& cpu, avr_instr_t i) { return instr_sbi<false>(cpu, i); }
uint32_t instr_merged_sbi(atmega32u4_t& cpu, avr_instr_t i) { return instr_sbi<true>(cpu, i); }

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_cbi(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.st_ior<merged>(i.dst, cpu.ld_ior<merged>(i.dst) & ~i.src);
    cpu.pc += 1;
    return 2;
}
uint32_t instr_cbi(atmega32u4_t& cpu, avr_instr_t i) { return instr_cbi<false>(cpu, i); }
uint32_t instr_merged_cbi(atmega32u4_t& cpu, avr_instr_t i) { return instr_cbi<true>(cpu, i); }

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_sbis(atmega32u4_t& cpu, avr_instr_t i)
{
    if(cpu.ld_ior<merged>(i.dst) & i.src)
    {
        if(next_instr_is_two_words(cpu))
        {
            cpu.pc += 3;
            return 3;
        }
        cpu.pc += 2;
        return 2;
    }
    cpu.pc += 1;
    return 1;
}
uint32_t instr_sbis(atmega32u4_t& cpu, avr_instr_t i) { return instr_sbis<false>(cpu, i); }
uint32_t instr_merged_sbis(atmega32u4_t& cpu, avr_instr_t i) { return instr_sbis<true>(cpu, i); }

template<bool merged>
ARDENS_FORCEINLINE static uint32_t instr_sbic(atmega32u4_t& cpu, avr_instr_t i)
{
    if(!(cpu.ld_ior<merged>(i.dst) & i.src))
    {
        if(next_instr_is_two_words(cpu))
        {
            cpu.pc += 3;
            return 3;
        }
        cpu.pc += 2;
        return 2;
    }
    cpu.pc += 1;
    return 1;
}
uint32_t instr_sbic(atmega32u4_t& cpu, avr_instr_t i) { return instr_sbic<false>(cpu, i); }
uint32_t instr_merged_sbic(atmega32u4_t& cpu, avr_instr_t i) { return instr_sbic<true>(cpu, i); }

uint32_t instr_sbrs(atmega32u4_t& cpu, avr_instr_t i)
{
    if(cpu.gpr(i.dst) & i.src)
    {
        if(next_instr_is_two_words(cpu))
        {
            cpu.pc += 3;
            return 3;
        }
        cpu.pc += 2;
        return 2;
    }
    cpu.pc += 1;
    return 1;
}

uint32_t instr_sbrc(atmega32u4_t& cpu, avr_instr_t i)
{
    if(!(cpu.gpr(i.dst) & i.src))
    {
        if(next_instr_is_two_words(cpu))
        {
            cpu.pc += 3;
            return 3;
        }
        cpu.pc += 2;
        return 2;
    }
    cpu.pc += 1;
    return 1;
}

uint32_t instr_bld(atmega32u4_t& cpu, avr_instr_t i)
{
    if(cpu.sreg() & SREG_T)
        cpu.gpr(i.dst) |= i.src;
    else
        cpu.gpr(i.dst) &= ~i.src;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_bst(atmega32u4_t& cpu, avr_instr_t i)
{
    set_flag(cpu, SREG_T, cpu.gpr(i.dst) & i.src);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_com(atmega32u4_t& cpu, avr_instr_t i)
{
    uint8_t src = cpu.gpr(i.dst);
    uint8_t res = ~src;
    cpu.gpr(i.dst) = res;
    set_flag(cpu, SREG_V, 0);
    set_flag(cpu, SREG_C, 1);
    set_flags_nzs(cpu, res);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_neg(atmega32u4_t& cpu, avr_instr_t i)
{
    uint8_t src = cpu.gpr(i.dst);
    uint8_t res = uint8_t(-src);
    cpu.gpr(i.dst) = res;

    set_flag(cpu, SREG_H, (res | src) & 0x8);
    set_flag(cpu, SREG_V, res == 0x80);
    set_flag(cpu, SREG_C, res != 0x00);
    set_flags_nzs(cpu, res);
    
    cpu.pc += 1;
    return 1;
}

uint32_t instr_swap(atmega32u4_t& cpu, avr_instr_t i)
{
    uint8_t dst = cpu.gpr(i.dst);
    dst = (dst >> 4) | (dst << 4);
    cpu.gpr(i.dst) = dst;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_inc(atmega32u4_t& cpu, avr_instr_t i)
{
    uint8_t dst = cpu.gpr(i.dst);
    uint8_t res = dst + 1;
    cpu.gpr(i.dst) = res;
    set_flag(cpu, SREG_V, res == 0x80);
    set_flags_nzs(cpu, res);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_dec(atmega32u4_t& cpu, avr_instr_t i)
{
    uint8_t dst = cpu.gpr(i.dst);
    uint8_t res = dst - 1;
    cpu.gpr(i.dst) = res;
    set_flag(cpu, SREG_V, dst == 0x80);
    set_flags_nzs(cpu, res);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_asr(atmega32u4_t& cpu, avr_instr_t i)
{
    uint8_t dst = cpu.gpr(i.dst);
    uint8_t res = (dst >> 1) | (dst & 0x80);
    cpu.gpr(i.dst) = res;
    set_flag(cpu, SREG_C, dst & 0x1);
    set_flag(cpu, SREG_V, (dst >> 7) ^ (dst & 1));
    set_flags_nzs(cpu, res);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_lsr(atmega32u4_t& cpu, avr_instr_t i)
{
    uint8_t dst = cpu.gpr(i.dst);
    uint8_t res = (dst >> 1);
    cpu.gpr(i.dst) = res;
    set_flag(cpu, SREG_C, dst & 0x1);
    set_flag(cpu, SREG_V, dst & 0x1);
    set_flag(cpu, SREG_S, dst & 0x1);
    set_flag(cpu, SREG_N, 0);
    set_flag(cpu, SREG_Z, res == 0);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_ror(atmega32u4_t& cpu, avr_instr_t i)
{
    uint8_t dst = cpu.gpr(i.dst);
    uint8_t res = dst >> 1;
    if(cpu.sreg() & SREG_C)
        res |= 0x80;
    cpu.gpr(i.dst) = res;
    set_flag(cpu, SREG_C, dst & 0x1);
    set_flag(cpu, SREG_V, (res >> 7) ^ (dst & 0x1));
    set_flags_nzs(cpu, res);
    cpu.pc += 1;
    return 1;
}

uint32_t instr_sleep(atmega32u4_t& cpu, avr_instr_t i)
{
    if(cpu.smcr() & 0x1)
        cpu.active = false;
    cpu.pc += 1;
    return 1;
}

uint32_t instr_mul(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t src = cpu.gpr(i.src);
    uint16_t dst = cpu.gpr(i.dst);
    uint16_t res = src * dst;
    cpu.gpr(0) = uint8_t(res >> 0);
    cpu.gpr(1) = uint8_t(res >> 8);
    set_flag(cpu, SREG_Z, res == 0);
    set_flag(cpu, SREG_C, res & 0x8000);
    cpu.pc += 1;
    return 2;
}

uint32_t instr_muls(atmega32u4_t& cpu, avr_instr_t i)
{
    int16_t src = (int8_t)cpu.gpr(i.src);
    int16_t dst = (int8_t)cpu.gpr(i.dst);
    int16_t res = src * dst;
    cpu.gpr(0) = uint8_t((uint16_t)res >> 0);
    cpu.gpr(1) = uint8_t((uint16_t)res >> 8);
    set_flag(cpu, SREG_Z, res == 0);
    set_flag(cpu, SREG_C, res & 0x8000);
    cpu.pc += 1;
    return 2;
}

uint32_t instr_mulsu(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t src = cpu.gpr(i.src);
    int16_t dst = (int8_t)cpu.gpr(i.dst);
    int16_t res = src * dst;
    cpu.gpr(0) = uint8_t((uint16_t)res >> 0);
    cpu.gpr(1) = uint8_t((uint16_t)res >> 8);
    set_flag(cpu, SREG_Z, res == 0);
    set_flag(cpu, SREG_C, res & 0x8000);
    cpu.pc += 1;
    return 2;
}

uint32_t instr_fmul(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t src = cpu.gpr(i.src);
    uint16_t dst = cpu.gpr(i.dst);
    uint16_t res = src * dst;
    cpu.gpr(0) = uint8_t((uint16_t)res << 1);
    cpu.gpr(1) = uint8_t((uint16_t)res >> 7);
    set_flag(cpu, SREG_Z, res == 0);
    set_flag(cpu, SREG_C, res & 0x8000);
    cpu.pc += 1;
    return 2;
}

uint32_t instr_fmuls(atmega32u4_t& cpu, avr_instr_t i)
{
    int16_t src = (int8_t)cpu.gpr(i.src);
    int16_t dst = (int8_t)cpu.gpr(i.dst);
    int16_t res = src * dst;
    cpu.gpr(0) = uint8_t((uint16_t)res << 1);
    cpu.gpr(1) = uint8_t((uint16_t)res >> 7);
    set_flag(cpu, SREG_Z, res == 0);
    set_flag(cpu, SREG_C, res & 0x8000);
    cpu.pc += 1;
    return 2;
}

uint32_t instr_fmulsu(atmega32u4_t& cpu, avr_instr_t i)
{
    uint16_t src = cpu.gpr(i.src);
    int16_t dst = (int8_t)cpu.gpr(i.dst);
    int16_t res = src * dst;
    cpu.gpr(0) = uint8_t((uint16_t)res << 1);
    cpu.gpr(1) = uint8_t((uint16_t)res >> 7);
    set_flag(cpu, SREG_Z, res == 0);
    set_flag(cpu, SREG_C, res & 0x8000);
    cpu.pc += 1;
    return 2;
}

uint32_t instr_merged_ldi2(atmega32u4_t& cpu, avr_instr_t i)
{
    cpu.gpr(i.dst) = i.src;
    cpu.gpr(i.m0) = i.m1;
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
