#pragma once

#define NOMINMAX

#include <vector>
#include <array>
#include <bitset>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <istream>
#include <ostream>
#include <algorithm>

#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "absim_instructions.hpp"

#if defined(_MSC_VER)
#define ARDENS_FORCEINLINE __forceinline
//#define ARDENS_FORCEINLINE
#elif defined(__GNUC__) || defined(__clang__) 
#define ARDENS_FORCEINLINE
#else
#define ARDENS_FORCEINLINE
#endif

#ifdef ARDENS_LLVM
namespace llvm
{
class DWARFContext;
namespace object { class ObjectFile; }
}
#endif

namespace absim
{

enum autobreak_t
{
    // special autobreak for the "break" instruction
    AB_BREAK,

    AB_STACK_OVERFLOW,
    AB_NULL_DEREF,
    AB_NULL_REL_DEREF,
    AB_OOB_DEREF,
    AB_OOB_EEPROM,
    AB_OOB_IJMP,
    AB_OOB_PC,
    AB_UNKNOWN_INSTR,
    AB_SPI_WCOL,
    AB_FX_BUSY,

    AB_NUM
};

struct int_vector_info_t
{
    char const* name;
    char const* desc;
};
extern std::array<int_vector_info_t, 43> const INT_VECTOR_INFO;

struct reg_info_t
{
    char const* name;
    std::array<char const*, 8> bits;
};
extern std::array<reg_info_t, 256> const REG_INFO;

struct avr_instr_t
{
    uint16_t word;
    uint8_t src;
    uint8_t dst;
    uint8_t func;

    // extra data for merged instructions
    uint8_t m0;
    uint8_t m1;
    uint8_t m2;
};

struct atmega32u4_t
{
    static constexpr size_t PROG_SIZE_BYTES = 32 * 1024;
    static constexpr size_t DATA_SIZE_BYTES = 2560 + 256;

    std::array<uint8_t, DATA_SIZE_BYTES> data;

    inline uint8_t& sfr(uint8_t n) { return data[n + 0x20]; }

    inline uint8_t& PINB() { return sfr(0x03); }
    inline uint8_t& DDRB() { return sfr(0x04); }
    inline uint8_t& PORTB() { return sfr(0x05); }
    inline uint8_t& PINC() { return sfr(0x06); }
    inline uint8_t& DDRC() { return sfr(0x07); }
    inline uint8_t& PORTC() { return sfr(0x08); }
    inline uint8_t& PIND() { return sfr(0x09); }
    inline uint8_t& DDRD() { return sfr(0x0a); }
    inline uint8_t& PORTD() { return sfr(0x0b); }
    inline uint8_t& PINE() { return sfr(0x0c); }
    inline uint8_t& DDRE() { return sfr(0x0d); }
    inline uint8_t& PORTE() { return sfr(0x0e); }
    inline uint8_t& PINF() { return sfr(0x0f); }
    inline uint8_t& DDRF() { return sfr(0x10); }
    inline uint8_t& PORTF() { return sfr(0x11); }
    inline uint8_t& TIFR0() { return sfr(0x15); }
    inline uint8_t& TIFR1() { return sfr(0x16); }
    inline uint8_t& TIFR3() { return sfr(0x18); }
    inline uint8_t& TIFR4() { return sfr(0x19); }
    inline uint8_t& PCIFR() { return sfr(0x1b); }
    inline uint8_t& EIFR() { return sfr(0x1c); }
    inline uint8_t& EIMSK() { return sfr(0x1d); }
    inline uint8_t& GPIOR0() { return sfr(0x1e); }
    inline uint8_t& EECR() { return sfr(0x1f); }
    inline uint8_t& EEDR() { return sfr(0x20); }
    inline uint8_t& EEARL() { return sfr(0x21); }
    inline uint8_t& EEARH() { return sfr(0x22); }
    inline uint8_t& GTCCR() { return sfr(0x23); }
    inline uint8_t& TCCR0A() { return sfr(0x24); }
    inline uint8_t& TCCR0B() { return sfr(0x25); }
    inline uint8_t& TCNT0() { return sfr(0x26); }
    inline uint8_t& OCR0A() { return sfr(0x27); }
    inline uint8_t& OCR0B() { return sfr(0x28); }
    inline uint8_t& PLLCSR() { return sfr(0x29); }
    inline uint8_t& GPIOR1() { return sfr(0x2a); }
    inline uint8_t& GPIOR2() { return sfr(0x2b); }
    inline uint8_t& SPCR() { return sfr(0x2c); }
    inline uint8_t& SPSR() { return sfr(0x2d); }
    inline uint8_t& SPDR() { return sfr(0x2e); }
    inline uint8_t& ACSR() { return sfr(0x30); }
    inline uint8_t& OCDR() { return sfr(0x31); }
    inline uint8_t& MONDR() { return sfr(0x31); }
    inline uint8_t& PLLFRQ() { return sfr(0x32); }
    inline uint8_t& SMCR() { return sfr(0x33); }
    inline uint8_t& MCUSR() { return sfr(0x34); }
    inline uint8_t& MCUCR() { return sfr(0x35); }
    inline uint8_t& SPMCSR() { return sfr(0x37); }
    inline uint8_t& SPL() { return sfr(0x3d); }
    inline uint8_t& SPH() { return sfr(0x3e); }
    inline uint8_t& SREG() { return sfr(0x3f); }
    inline uint8_t& WDTCSR() { return sfr(0x40); }
    inline uint8_t& CLKPR() { return sfr(0x41); }
    inline uint8_t& PRR0() { return sfr(0x44); }
    inline uint8_t& PRR1() { return sfr(0x45); }
    inline uint8_t& OSCCAL() { return sfr(0x46); }
    inline uint8_t& RCCTRL() { return sfr(0x47); }
    inline uint8_t& PCICR() { return sfr(0x48); }
    inline uint8_t& EICRA() { return sfr(0x49); }
    inline uint8_t& EICRB() { return sfr(0x4a); }
    inline uint8_t& PCMSK0() { return sfr(0x4b); }
    inline uint8_t& TIMSK0() { return sfr(0x4e); }
    inline uint8_t& TIMSK1() { return sfr(0x4f); }
    inline uint8_t& TIMSK3() { return sfr(0x51); }
    inline uint8_t& TIMSK4() { return sfr(0x52); }
    inline uint8_t& ADCL() { return sfr(0x58); }
    inline uint8_t& ADCH() { return sfr(0x59); }
    inline uint8_t& ADCSRA() { return sfr(0x5a); }
    inline uint8_t& ADCSRB() { return sfr(0x5b); }
    inline uint8_t& ADMUX() { return sfr(0x5c); }
    inline uint8_t& DIDR2() { return sfr(0x5d); }
    inline uint8_t& DIDR0() { return sfr(0x5e); }
    inline uint8_t& DIDR1() { return sfr(0x5f); }
    inline uint8_t& TCCR1A() { return sfr(0x60); }
    inline uint8_t& TCCR1B() { return sfr(0x61); }
    inline uint8_t& TCCR1C() { return sfr(0x62); }
    inline uint8_t& TCNT1L() { return sfr(0x64); }
    inline uint8_t& TCNT1H() { return sfr(0x65); }
    inline uint8_t& ICR1L() { return sfr(0x66); }
    inline uint8_t& ICR1H() { return sfr(0x67); }
    inline uint8_t& OCR1AL() { return sfr(0x68); }
    inline uint8_t& OCR1AH() { return sfr(0x69); }
    inline uint8_t& OCR1BL() { return sfr(0x6a); }
    inline uint8_t& OCR1BH() { return sfr(0x6b); }
    inline uint8_t& OCR1CL() { return sfr(0x6c); }
    inline uint8_t& OCR1CH() { return sfr(0x6d); }
    inline uint8_t& TCCR3A() { return sfr(0x70); }
    inline uint8_t& TCCR3B() { return sfr(0x71); }
    inline uint8_t& TCCR3C() { return sfr(0x72); }
    inline uint8_t& TCNT3L() { return sfr(0x74); }
    inline uint8_t& TCNT3H() { return sfr(0x75); }
    inline uint8_t& ICR3L() { return sfr(0x76); }
    inline uint8_t& ICR3H() { return sfr(0x77); }
    inline uint8_t& OCR3AL() { return sfr(0x78); }
    inline uint8_t& OCR3AH() { return sfr(0x79); }
    inline uint8_t& OCR3BL() { return sfr(0x7a); }
    inline uint8_t& OCR3BH() { return sfr(0x7b); }
    inline uint8_t& OCR3CL() { return sfr(0x7c); }
    inline uint8_t& OCR3CH() { return sfr(0x7d); }
    inline uint8_t& TWBR() { return sfr(0x98); }
    inline uint8_t& TWSR() { return sfr(0x99); }
    inline uint8_t& TWAR() { return sfr(0x9a); }
    inline uint8_t& TWDR() { return sfr(0x9b); }
    inline uint8_t& TWCR() { return sfr(0x9c); }
    inline uint8_t& TWAMR() { return sfr(0x9d); }
    inline uint8_t& TCNT4() { return sfr(0x9e); }
    inline uint8_t& TC4H() { return sfr(0x9f); }
    inline uint8_t& TCCR4A() { return sfr(0xa0); }
    inline uint8_t& TCCR4B() { return sfr(0xa1); }
    inline uint8_t& TCCR4C() { return sfr(0xa2); }
    inline uint8_t& TCCR4D() { return sfr(0xa3); }
    inline uint8_t& TCCR4E() { return sfr(0xa4); }
    inline uint8_t& CLKSEL0() { return sfr(0xa5); }
    inline uint8_t& CLKSEL1() { return sfr(0xa6); }
    inline uint8_t& CLKSTA() { return sfr(0xa7); }
    inline uint8_t& UCSR1A() { return sfr(0xa8); }
    inline uint8_t& UCSR1B() { return sfr(0xa9); }
    inline uint8_t& UCSR1C() { return sfr(0xaa); }
    inline uint8_t& UCSR1D() { return sfr(0xab); }
    inline uint8_t& UBRR1L() { return sfr(0xac); }
    inline uint8_t& UBRR1H() { return sfr(0xad); }
    inline uint8_t& UDR1() { return sfr(0xae); }
    inline uint8_t& OCR4A() { return sfr(0xaf); }
    inline uint8_t& OCR4B() { return sfr(0xb0); }
    inline uint8_t& OCR4C() { return sfr(0xb1); }
    inline uint8_t& OCR4D() { return sfr(0xb2); }
    inline uint8_t& DT4() { return sfr(0xb4); }
    inline uint8_t& UHWCON() { return sfr(0xb7); }
    inline uint8_t& USBCON() { return sfr(0xb8); }
    inline uint8_t& USBSTA() { return sfr(0xb9); }
    inline uint8_t& USBINT() { return sfr(0xba); }
    inline uint8_t& UDCON() { return sfr(0xc0); }
    inline uint8_t& UDINT() { return sfr(0xc1); }
    inline uint8_t& UDIEN() { return sfr(0xc2); }
    inline uint8_t& UDADDR() { return sfr(0xc3); }
    inline uint8_t& UDFNUML() { return sfr(0xc4); }
    inline uint8_t& UDFNUMH() { return sfr(0xc5); }
    inline uint8_t& UDMFN() { return sfr(0xc6); }
    inline uint8_t& UEINTX() { return sfr(0xc8); }
    inline uint8_t& UENUM() { return sfr(0xc9); }
    inline uint8_t& UERST() { return sfr(0xca); }
    inline uint8_t& UECONX() { return sfr(0xcb); }
    inline uint8_t& UECFG0X() { return sfr(0xcc); }
    inline uint8_t& UECFG1X() { return sfr(0xcd); }
    inline uint8_t& UESTA0X() { return sfr(0xce); }
    inline uint8_t& UESTA1X() { return sfr(0xcf); }
    inline uint8_t& UEIENX() { return sfr(0xd0); }
    inline uint8_t& UEDATX() { return sfr(0xd1); }
    inline uint8_t& UEBCLX() { return sfr(0xd2); }
    inline uint8_t& UEBCHX() { return sfr(0xd3); }
    inline uint8_t& UEINT() { return sfr(0xd4); }

    inline uint8_t& gpr(uint8_t n)
    {
        assert(n < 32);
        return data[n];
    }

    uint32_t just_read;
    uint32_t just_written;

    using ld_handler_t = uint8_t(*)(atmega32u4_t& cpu, uint16_t ptr);
    using st_handler_t = void(*)(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    std::array<ld_handler_t, 256> ld_handlers;
    std::array<st_handler_t, 256> st_handlers;

    ARDENS_FORCEINLINE uint8_t ld(uint16_t ptr)
    {
        check_deref(ptr);
        just_read = ptr;
        if(ptr < ld_handlers.size() && ld_handlers[ptr])
            return ld_handlers[ptr](*this, ptr);
        return ptr < data.size() ? data[ptr] : 0x00;
    }
    ARDENS_FORCEINLINE void st(uint16_t ptr, uint8_t x)
    {
        check_deref(ptr);
        just_written = ptr;
        if(ptr < st_handlers.size() && st_handlers[ptr])
            return st_handlers[ptr](*this, ptr, x);
        if(ptr < data.size()) data[ptr] = x;
    }

    ARDENS_FORCEINLINE uint8_t ld_ior(uint8_t n)
    {
        return ld(n + 32);
    }
    ARDENS_FORCEINLINE void st_ior(uint8_t n, uint8_t x)
    {
        st(n + 32, x);
    }

    ARDENS_FORCEINLINE uint16_t gpr_word(uint8_t n)
    {
        uint16_t lo = gpr(n + 0);
        uint16_t hi = gpr(n + 1);
        return lo | (hi << 8);
    }

    ARDENS_FORCEINLINE uint16_t w_word() { return gpr_word(24); }
    ARDENS_FORCEINLINE uint16_t x_word() { return gpr_word(26); }
    ARDENS_FORCEINLINE uint16_t y_word() { return gpr_word(28); }
    ARDENS_FORCEINLINE uint16_t z_word() { return gpr_word(30); }

    // false if the cpu is sleeping
    bool active;
    uint8_t wakeup_cycles; // for tracking interrupt wakeup delay
    bool just_interrupted;

    uint8_t& smcr() { return data[0x53]; }
    uint8_t& mcucr() { return data[0x55]; }
    uint8_t& spl() { return data[0x5d]; }
    uint8_t& sph() { return data[0x5e]; }
    uint8_t& sreg() { return data[0x5f]; }

    uint8_t& tifr0() { return data[0x35]; }
    uint8_t& tifr1() { return data[0x36]; }
    uint8_t& tifr3() { return data[0x38]; }
    uint8_t& tifr4() { return data[0x39]; }

    uint8_t& timsk0() { return data[0x6e]; }
    uint8_t& timsk1() { return data[0x6f]; }
    uint8_t& timsk3() { return data[0x71]; }
    uint8_t& timsk4() { return data[0x72]; }

    uint8_t& tccr0a() { return data[0x44]; }
    uint8_t& tccr0b() { return data[0x45]; }
    uint8_t& tcnt0() { return data[0x46]; }
    uint8_t& ocr0a() { return data[0x47]; }
    uint8_t& ocr0b() { return data[0x48]; }

    ARDENS_FORCEINLINE uint16_t sp()
    {
        return (uint16_t)spl() | ((uint16_t)sph() << 8);
    }

    ARDENS_FORCEINLINE uint16_t sp() const
    {
        return (uint16_t)data[0x5d] | ((uint16_t)data[0x5e] << 8);
    }

    uint32_t min_stack; // lowest value for SP
    uint32_t stack_check; // max allowable value for SP
    bool pushed_at_least_once;

    std::bitset<AB_NUM> autobreaks;
    std::bitset<AB_NUM> enabled_autobreaks;
    //autobreak_t autobreak;
    //std::array<bool, AB_NUM> enable_autobreaks;
    ARDENS_FORCEINLINE void autobreak(autobreak_t t)
    {
        (void)t;
#ifndef ARDENS_NO_DEBUGGER
        autobreaks.set(t);
#endif
    }
    ARDENS_FORCEINLINE bool should_autobreak() const
    {
#ifndef ARDENS_NO_DEBUGGER
        return autobreaks.any() && (autobreaks & enabled_autobreaks).any();
#else
        return false;
#endif
    }
    ARDENS_FORCEINLINE bool should_autobreak_gui() const
    {
#ifndef ARDENS_NO_DEBUGGER
        auto ab = autobreaks;
        ab.reset(AB_BREAK);
        return (ab & enabled_autobreaks).any();
#else
        return false;
#endif
    }

    ARDENS_FORCEINLINE void check_deref(uint16_t addr)
    {
        (void)addr;
#ifndef ARDENS_NO_DEBUGGER
        if(addr == 0)
            autobreak(AB_NULL_DEREF);
        else if(addr >= data.size())
            autobreak(AB_OOB_DEREF);
#endif
    }

    ARDENS_FORCEINLINE void check_stack_overflow(uint16_t tsp)
    {
        (void)tsp;
#ifndef ARDENS_NO_DEBUGGER
        if(!pushed_at_least_once) return;
        // check min stack
        min_stack = std::min<uint32_t>(min_stack, tsp);
        if(tsp < stack_check)
            autobreak(AB_STACK_OVERFLOW);
#endif
    }

    ARDENS_FORCEINLINE void check_stack_overflow()
    {
#ifndef ARDENS_NO_DEBUGGER
        check_stack_overflow(sp());
#endif
    }

    ARDENS_FORCEINLINE void push(uint8_t x)
    {
        uint16_t tsp = sp();
        st(tsp, x);
        --tsp;
        spl() = uint8_t(tsp >> 0);
        sph() = uint8_t(tsp >> 8);
        pushed_at_least_once = true;
        check_stack_overflow(tsp);
    }

    ARDENS_FORCEINLINE uint8_t pop()
    {
        uint16_t tsp = sp();
        ++tsp;
        uint8_t x = ld(tsp);
        spl() = uint8_t(tsp >> 0);
        sph() = uint8_t(tsp >> 8);
        return x;
    }

    std::array<uint8_t, PROG_SIZE_BYTES> prog; // program flash memory
    std::array<uint8_t, 1024>  eeprom; // EEPROM
    std::bitset<1024> eeprom_modified_bytes;
    bool eeprom_modified;
    bool eeprom_dirty;

    // SREG after previous instruction (used for interrupt bit)
    uint8_t prev_sreg;

    uint16_t pc;                       // program counter

    uint16_t executing_instr_pc;

    static constexpr size_t MAX_STACK_FRAMES = 1280;
    struct stack_frame_t { uint16_t pc; uint16_t sp; };
    std::array<stack_frame_t, MAX_STACK_FRAMES> stack_frames;
    uint32_t num_stack_frames;
    ARDENS_FORCEINLINE void push_stack_frame(uint16_t ret_addr)
    {
        (void)ret_addr;
#ifndef ARDENS_NO_DEBUGGER
        if(num_stack_frames < stack_frames.size())
            stack_frames[num_stack_frames++] = { ret_addr, sp() };
#endif
    }
    ARDENS_FORCEINLINE void pop_stack_frame()
    {
#ifndef ARDENS_NO_DEBUGGER
        auto tsp = sp();
        while(num_stack_frames > 0)
        {
            auto f = stack_frames[num_stack_frames - 1];
            if(f.sp > tsp) break;
            --num_stack_frames;
        }
#endif
    }

    static constexpr int MAX_INSTR_CYCLES = 32;

    uint16_t last_addr;
    uint16_t num_instrs;
    uint16_t num_instrs_total;
    bool no_merged;
    std::array<avr_instr_t, PROG_SIZE_BYTES / 2> decoded_prog;
    std::array<avr_instr_t, PROG_SIZE_BYTES / 2> merged_prog; // decoded and merged instrs
    std::array<disassembled_instr_t, PROG_SIZE_BYTES / 2> disassembled_prog;
    bool program_loaded;
    bool decoded;
    void decode();
    void merge_instrs();
    size_t addr_to_disassembled_index(uint16_t addr);

    static void st_handle_pin(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void st_handle_port(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    static void st_handle_prr0(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    // timer0
    struct timer8_t
    {
        uint64_t prev_update_cycle;
        uint64_t next_update_cycle;
        uint32_t prescaler_cycle;
        uint32_t divider;
        uint32_t top;
        uint32_t tov;
        uint32_t tcnt;
        uint32_t ocrNa;
        uint32_t ocrNb;
        bool phase_correct;
        bool count_down;
        bool update_ocrN_at_top;
        template<class A> void serialize(A& a)
        {
            a(prev_update_cycle, next_update_cycle);
            a(prescaler_cycle, divider);
            a(top, tov, tcnt, ocrNa, ocrNb);
            a(phase_correct, count_down, update_ocrN_at_top);
        }
    };
    timer8_t timer0;
    static void timer0_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer0_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer0_handle_st_tcnt(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static uint8_t timer0_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr);
    void update_timer0();

    // timer 1 or 3
    struct timer16_t
    {
        uint64_t prev_update_cycle;
        uint64_t next_update_cycle;
        uint32_t prescaler_cycle;
        uint32_t divider;
        uint32_t top;
        uint32_t tov;
        uint32_t tcnt;
        uint32_t ocrNa;
        uint32_t ocrNb;
        uint32_t ocrNc;
        uint32_t tifrN_addr;
        uint32_t timskN_addr;
        uint32_t prr_addr;
        uint32_t prr_mask;
        uint32_t base_addr;
        uint32_t com3a;
        uint8_t temp; // reg for 16-bit access
        bool phase_correct;
        bool count_down;
        bool update_ocrN_at_top;
        bool update_ocrN_at_bottom;
        template<class A> void serialize(A& a)
        {
            a(prev_update_cycle, next_update_cycle);
            a(prescaler_cycle, divider);
            a(top, tov, tcnt, ocrNa, ocrNb, ocrNc);
            a(tifrN_addr, timskN_addr, prr_addr, prr_mask, base_addr);
            a(temp);
            a(phase_correct, count_down);
            a(update_ocrN_at_top, update_ocrN_at_bottom);
        }
    };
    static void timer1_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer3_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer1_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer3_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static uint8_t timer1_handle_ld_regs(atmega32u4_t& cpu, uint16_t ptr);
    static uint8_t timer3_handle_ld_regs(atmega32u4_t& cpu, uint16_t ptr);
    void update_timer1();
    void update_timer3();

    timer16_t timer1;
    timer16_t timer3;

    // timer 4
    struct timer10_t
    {
        uint64_t prev_update_cycle;
        uint64_t next_update_cycle;
        uint32_t divider_cycle;
        uint32_t divider;
        uint32_t top;
        uint32_t tov;
        uint32_t tcnt;
        uint32_t ocrNa_next;
        uint32_t ocrNb_next;
        uint32_t ocrNc_next;
        uint32_t ocrNd_next;
        uint32_t ocrNa;
        uint32_t ocrNb;
        uint32_t ocrNc;
        uint32_t ocrNd;
        uint32_t async_cycle;
        uint32_t com4a;
        bool tlock;
        bool enhc;
        bool phase_correct;
        bool count_down;
        bool update_ocrN_at_top;
        bool update_ocrN_at_bottom;
        template<class A> void serialize(A& a)
        {
            a(prev_update_cycle, next_update_cycle);
            a(divider_cycle, divider);
            a(top, tov, tcnt);
            a(ocrNa, ocrNa_next);
            a(ocrNb, ocrNb_next);
            a(ocrNc, ocrNc_next);
            a(ocrNd, ocrNd_next);
            a(async_cycle, com4a);
            a(tlock, enhc, phase_correct, count_down);
            a(update_ocrN_at_top, update_ocrN_at_bottom);
        }
    };
    static void timer4_handle_st_ocrN(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer4_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer4_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static uint8_t timer4_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr);
    void update_timer4();
    timer10_t timer4;

    // LEDs
    uint8_t led_tx() const;
    uint8_t led_rx() const;
    void led_rgb(uint8_t& r, uint8_t& g, uint8_t& b) const;

    // PLL
    uint64_t pll_lock_cycle;
    uint32_t pll_num12; // numerator /12 of pll cycles per main cycle
    bool pll_busy;
    void cycle_pll(uint32_t cycles);
    static void pll_handle_st_pllcsr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    // SPI
    bool spsr_read_after_transmit;
    bool spi_busy;
    bool spi_busy_clear;
    bool spi_latch_read;
    bool spi_data_latched;
    uint8_t spi_data_byte;
    uint8_t spi_datain_byte;
    uint64_t spi_done_cycle;
    uint64_t spi_transmit_zero_cycle;
    uint32_t spi_clock_cycles;
    void update_spi();
    static void spi_handle_st_spcr_or_spsr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void spi_handle_st_spdr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static uint8_t spi_handle_ld_spsr(atmega32u4_t& cpu, uint16_t ptr);
    static uint8_t spi_handle_ld_spdr(atmega32u4_t& cpu, uint16_t ptr);

    // EEPROM
    uint32_t eeprom_clear_eempe_cycles;
    uint32_t eeprom_write_addr;
    uint32_t eeprom_write_data;
    uint32_t eeprom_program_cycles;
    bool eeprom_busy;
    void cycle_eeprom(uint32_t cycles);
    static void eeprom_handle_st_eecr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    // ADC
    uint32_t adc_prescaler_cycle;
    uint32_t adc_cycle;
    uint32_t adc_ref;
    uint32_t adc_result;
    uint32_t adc_seed;
    bool adc_busy;
    bool adc_nondeterminism;
    void cycle_adc(uint32_t cycles);
    void adc_handle_prr0(uint8_t x);
    static void adc_st_handle_adcsra(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    // sound
    static constexpr int SOUND_CYCLES = 320;
    static constexpr int16_t SOUND_GAIN = 2000;
    uint32_t sound_cycle;
    uint32_t sound_enabled; // bitmask of pins 1 and 2
    bool sound_pwm;
    int16_t sound_pwm_val;
    std::vector<int16_t> sound_buffer;
    static void sound_st_handler_ddrc(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    void cycle_sound(uint32_t cycles);

    // serial / USB
    std::vector<uint8_t> serial_bytes;
    struct usb_endpoint_t
    {
        uint8_t uenum;
        uint8_t ueintx;
        uint8_t uerst;
        uint8_t ueconx;
        uint8_t uecfg0x;
        uint8_t uecfg1x;
        uint8_t uesta0x;
        uint8_t uesta1x;
        uint8_t ueienx;
        uint8_t uebclx;
        uint8_t uebchx;
        std::vector<uint8_t> buffer; // TODO: DPRAM modeling
        uint32_t start;
        uint32_t length;
        void configure();
        uint8_t read(atmega32u4_t& cpu);
        void write(atmega32u4_t& cpu, uint8_t x);
        void copy_regs(atmega32u4_t& cpu);
        template<class A> void serialize(A& a)
        {
            a(uenum, ueintx, uerst, ueconx);
            a(uesta0x, uesta1x);
            a(uecfg0x, uecfg1x);
            a(ueienx, uebclx, uebchx);
            a(buffer, start, length);
        }
    };
    std::array<usb_endpoint_t, 8> usb_ep;
    uint64_t usb_next_sofi_cycle;
    uint64_t usb_next_eorsti_cycle;
    uint64_t usb_next_setconf_cycle;
    uint64_t usb_next_setlength_cycle;
    uint64_t usb_next_update_cycle;
    std::array<uint8_t, 832> usb_dpram;
    bool usb_attached;
    static void usb_st_handler(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static uint8_t usb_ld_handler_uedatx(atmega32u4_t& cpu, uint16_t ptr);
    void update_usb();
    void reset_usb();

    // SPM
    uint64_t spm_prev_cycle;
    bool spm_busy;
    uint8_t spm_en_cycles;
    enum
    {
        SPM_OP_NONE,
        SPM_OP_PAGE_LOAD,
        SPM_OP_PAGE_ERASE,
        SPM_OP_PAGE_WRITE,
        SPM_OP_BLB_SET,
        SPM_OP_RWW_EN,
        SPM_OP_SIG_READ,
    };
    uint8_t spm_op;
    uint32_t spm_cycles;
    std::array<uint8_t, 128> spm_buffer;
    static void st_handle_spmcsr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    void execute_spm();
    void update_spm();
    void erase_spm_buffer() { memset(spm_buffer.data(), 0xff, spm_buffer.size()); }

    // watchdog
    uint32_t watchdog_divider;
    uint32_t watchdog_divider_cycle;
    uint64_t watchdog_prev_cycle;
    uint64_t watchdog_next_cycle;
    static void st_handle_mcucr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void st_handle_mcusr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void st_handle_wdtcsr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    void update_watchdog_prescaler();
    void update_watchdog();

    bool check_interrupt(uint8_t vector, uint8_t flag, uint8_t& tifr);

    uint64_t cycle_count;

    // lock bits
    uint8_t lock;

    // fuses
    uint8_t fuse_lo;
    uint8_t fuse_hi;
    uint8_t fuse_ext;

    inline bool HWBE   () const { return (fuse_ext & (1 << 3)) == 0; }

    inline bool OCDEN  () const { return (fuse_hi  & (1 << 7)) == 0; }
    inline bool JTAGEN () const { return (fuse_hi  & (1 << 6)) == 0; }
    inline bool SPIEN  () const { return (fuse_hi  & (1 << 5)) == 0; }
    inline bool WDTON  () const { return (fuse_hi  & (1 << 4)) == 0; }
    inline bool EESAVE () const { return (fuse_hi  & (1 << 3)) == 0; }
    inline bool BOOTRST() const { return (fuse_hi  & (1 << 0)) == 0; }

    inline bool CKDIV8 () const { return (fuse_lo  & (1 << 7)) == 0; }
    inline bool CKOUT  () const { return (fuse_lo  & (1 << 6)) == 0; }

    inline uint8_t BOOTSZ() const { return (fuse_hi >> 1) & 0x3; }
    inline uint8_t SUT   () const { return (fuse_lo >> 4) & 0x3; }
    inline uint8_t CLKSEL() const { return (fuse_lo >> 0) & 0xf; }
    
    inline uint16_t bootloader_address() const
    {
        static uint16_t const ADDRS[4] = { 0x3800, 0x3c00, 0x3e00, 0x3f00 };
        return ADDRS[BOOTSZ() & 3];
    }

    void reset();
    void soft_reset(); // for WDT

    // execute at least one cycle (return how many cycles were executed)
    uint32_t advance_cycle();

    // update delayed peripheral states
    void update_all();
};

struct display_t
{
    std::array<uint8_t, 8192> filtered_pixels;
    std::array<uint16_t, 8192> filtered_pixel_counts;

    enum type_t
    {
        SSD1306,
        SSD1309,
        SH1106,
    };
    type_t type;

    // moving average
    static constexpr int MAX_PIXEL_HISTORY = 4;
    std::array<std::array<uint8_t, 8192>, MAX_PIXEL_HISTORY> pixels;
    int pixel_history_index;
    bool enable_filter;

    // physical display RAM
    std::array<uint8_t, 1024> ram;

    // segment driver current at 0xff contrast in mA
    // Arduboy: 195uA (0.195)
    float ref_segment_current;
    float current_limit_slope;
    static constexpr float MAX_DRIVER_CURRENT = 15.f;
    bool enable_current_limiting;
    float prev_row_drive;

    uint8_t contrast;
    bool entire_display_on;
    bool inverse_display;
    bool display_on;
    bool enable_charge_pump;

    // memory addressing mode
    enum class addr_mode
    {
        HORIZONTAL,
        VERTICAL,
        PAGE,
        INVALID
    } addressing_mode;

    // for horizontal or vertical addressing mode
    uint8_t col_start;
    uint8_t col_end;
    uint8_t page_start;
    uint8_t page_end;

    uint8_t mux_ratio;

    uint8_t display_offset;
    uint8_t display_start;

    bool com_scan_direction; // true: right-to-left
    bool alternative_com;
    bool com_remap;
    bool segment_remap;

    // Fosc values for each of the 16 command settings
    uint8_t fosc_index;
    uint8_t divide_ratio;
    uint8_t phase_1;
    uint8_t phase_2;
    uint8_t vcomh_deselect;

    double fosc() const;
    double refresh_rate() const;

    void reset();

    void update_clocking();

    // display refresh state
    uint8_t row;
    uint8_t row_cycle;
    uint8_t cycles_per_row;
    uint64_t ps_per_clk;

    void update_pixels_row();
    void filter_pixels();

    uint64_t ps_rem;

    bool processing_command;
    uint8_t current_command;
    uint8_t command_byte_index;

    // data address state
    uint8_t data_page;
    uint8_t data_col;

    // whether a vsync has just occurred
    bool vsync;

    void send_data(uint8_t byte);
    void send_command(uint8_t byte);

    // advance controller state by a given time
    // returns true if vsync occurred
    bool advance(uint64_t ps);
};

struct w25q128_t
{
    static constexpr size_t DATA_BYTES = 16 * 1024 * 1024;
    std::array<uint8_t, DATA_BYTES> data;

    static constexpr size_t NUM_SECTORS = DATA_BYTES / 4096;
    std::bitset<NUM_SECTORS> sectors_modified;
    bool sectors_dirty;

    bool enabled;
    bool woken_up;
    bool write_enabled;
    bool reading_status;
    bool processing_command;
    uint8_t reading;
    uint8_t programming;
    uint8_t erasing_sector;
    uint8_t releasing;
    uint8_t reading_jedec_id;
    uint64_t busy_ps_rem;
    uint32_t current_addr;

    enum
    {
        CMD_NONE,
        CMD_RELEASE_POWER_DOWN,
        CMD_PAGE_PROGRAM,
        CMD_READ_DATA,
        CMD_WRITE_DISABLE,
        CMD_READ_STATUS_REGISTER_1,
        CMD_WRITE_ENABLE,
        CMD_SECTOR_ERASE,
        CMD_JEDEC_ID,
        CMD_UNKNOWN,
        NUM_CMDS,
    };

    uint32_t command;

    uint32_t min_page;
    uint32_t max_page;

    bool busy_error;

    void reset();
    void erase_all_data();

    void advance(uint64_t ps);

    void set_enabled(bool e);
    uint8_t spi_transceive(uint8_t byte);
    void track_page();
};

struct elf_data_symbol_t
{
    std::string name;
    uint16_t addr;
    uint16_t size;
    uint16_t color_index;
    bool weak;
    bool global;
    bool notype;
    bool object;
};

struct icompare
{
    static char lower(char c)
    {
        if(c >= 'A' && c <= 'Z') return c + 'a' - 'A';
        return c;
    }
    int compare(std::string const& a, std::string const& b) const
    {
        auto sa = a.size();
        auto sb = b.size();
        auto s = (sa < sb ? sa : sb);
        for(size_t i = 0; i < s; ++i)
        {
            char ca = lower(a[i]);
            char cb = lower(b[i]);
            if(ca < cb) return -1;
            if(ca > cb) return 1;
        }
        if(sa < sb) return -1;
        if(sb > sa) return 1;
        return 0;
    }
    bool operator()(std::string const& a, std::string const& b) const
    {
        return compare(a, b) < 0;
    }
};

struct elf_data_t
{

    uint16_t data_begin;
    uint16_t data_end;
    uint16_t bss_begin;
    uint16_t bss_end;
    using map_type = std::map<uint16_t, elf_data_symbol_t>;
    map_type text_symbols;
    map_type data_symbols;
    std::vector<uint16_t> text_symbols_sorted;
    std::vector<uint16_t> data_symbols_sorted;
    std::vector<uint16_t> data_symbols_sorted_size;

    struct source_file_t
    {
        std::string filename;
        std::vector<std::string> lines;
    };

    std::vector<source_file_t> source_files;
    std::unordered_map<std::string, int> source_file_names;
    using source_line = std::pair<int, int>; // file index, line
    std::unordered_map<uint16_t, source_line> source_lines;

    std::vector<disassembled_instr_t> asm_with_source;
    size_t addr_to_disassembled_index(uint16_t addr);

    std::vector<char> fdata;
#ifdef ARDENS_LLVM
    std::unique_ptr<llvm::object::ObjectFile> obj;
    std::unique_ptr<llvm::DWARFContext> dwarf_ctx;
#endif

    struct global_t
    {
        uint64_t cu_offset; // compile unit
        int file, line;
        uint32_t addr;
        uint32_t type; // DIE offset
        bool text;
    };
    std::map<std::string, global_t, icompare> globals;

    ~elf_data_t(); // only exists for unique_ptr's above

};

// game save data
struct savedata_t
{
    uint64_t game_hash;
    std::vector<uint8_t> eeprom;
    std::map<uint32_t, std::array<uint8_t, 4096>> fx_sectors;
    std::bitset<1024> eeprom_modified_bytes;

    template<class A> void serialize(A& a)
    {
        a(game_hash, eeprom, fx_sectors);
        a(eeprom_modified_bytes);
    }
    void clear()
    {
        game_hash = 0;
        eeprom.clear();
        fx_sectors.clear();
        eeprom_modified_bytes.reset();
    }
};

struct arduboy_config_t
{
    display_t::type_t display_type = display_t::type_t::SSD1306;
    uint8_t fxport_reg = 0x2d;    // PORTD
    uint8_t fxport_mask = 1 << 1; // PORTD1
    bool bootloader = true;
    bool boot_to_menu = false;
};

struct arduboy_t
{
    atmega32u4_t cpu;
    display_t display;
    w25q128_t fx;

    bool prev_display_reset;

    uint8_t fxport_reg;
    uint8_t fxport_mask;

    uint64_t game_hash;
    void update_game_hash();

    std::string title;
    std::string device_type;
    std::string prog_filename;
    std::vector<uint8_t> prog_filedata;
    
    std::vector<uint8_t> fxdata;
    std::vector<uint8_t> fxsave;
    void reload_fx();

    std::unique_ptr<elf_data_t> elf;
    elf_data_symbol_t const* symbol_for_prog_addr(uint16_t addr);
    elf_data_symbol_t const* symbol_for_data_addr(uint16_t addr);

    static constexpr size_t NUM_INSTRS = atmega32u4_t::PROG_SIZE_BYTES / 2;

    std::array<uint64_t, NUM_INSTRS> profiler_counts;
    uint64_t profiler_total;
    uint64_t profiler_total_with_sleep;

    // counts for previous frame
    uint64_t prev_profiler_total;
    uint64_t prev_profiler_total_with_sleep;
    uint64_t prev_frame_cycles;
    uint32_t total_frames;
    uint32_t total_ms;
    uint32_t frame_bytes_total;
    uint32_t frame_bytes;
    std::vector<float> frame_cpu_usage;

    // time-based cpu usage
    std::vector<float> ms_cpu_usage_raw;
    std::vector<float> ms_cpu_usage;
    uint64_t prev_profiler_total_ms;
    uint64_t prev_profiler_total_with_sleep_ms;
    uint64_t prev_ms_cycles;

    bool profiler_enabled;
    
    uint64_t cached_profiler_total;
    uint64_t cached_profiler_total_with_sleep;
    struct hotspot_t
    {
        uint64_t count;
        uint16_t begin, end;
        template <class A> void serialize(A& a) { a(count, begin, end); }
    };
    std::array<hotspot_t, NUM_INSTRS> profiler_hotspots;
    uint32_t num_hotspots;
    std::vector<hotspot_t> profiler_hotspots_symbol;

    void profiler_build_hotspots();
    void profiler_reset();

    // breakpoints
    std::bitset<NUM_INSTRS> breakpoints;
    std::bitset<atmega32u4_t::DATA_SIZE_BYTES> breakpoints_rd;
    std::bitset<atmega32u4_t::DATA_SIZE_BYTES> breakpoints_wr;
    bool allow_nonstep_breakpoints;
    uint32_t break_step;

    uint64_t ps_rem;

    // paused at breakpoint
    bool paused;

    // saved data
    savedata_t savedata;
    bool savedata_dirty;
    bool load_savedata(std::istream& f);
    void save_savedata(std::ostream& f);

    arduboy_config_t cfg;
    bool flashcart_loaded;
    void reset();

    // advance at least one cycle (returns how many cycles were advanced)
    uint32_t cycle();

    void advance_instr();

    // each cycle is 62.5 ns
    static constexpr uint64_t CYCLE_PS = 62500;
    static constexpr uint64_t PS_BUFFER = CYCLE_PS * 256 * 64;

    // advance by specified number of picoseconds
    // ratio is for display filtering: 1.0 means real time,
    // 0.1 means 10x slower
    void advance(uint64_t ps);

    // returns an error string on error or empty string on success
    std::string load_file(char const* filename, std::istream& f, bool save = false);

    std::string load_bootloader_hex(std::istream& f);
    std::string load_bootloader_hex(uint8_t const* data, size_t size);
    std::string load_flashcart_zip(uint8_t const* data, size_t size);

    // snapshots contain full debugger and device state and are compressed
    bool save_snapshot(std::ostream& f);
    std::string load_snapshot(std::istream& f);

    // savestates only contain device state and are not compressed (e.g., for RetroArch)
    bool save_savestate(std::ostream& f);
    std::string load_savestate(std::istream& f);
};


constexpr uint8_t SREG_I = 1 << 7;
constexpr uint8_t SREG_T = 1 << 6;
constexpr uint8_t SREG_H = 1 << 5;
constexpr uint8_t SREG_S = 1 << 4;
constexpr uint8_t SREG_V = 1 << 3;
constexpr uint8_t SREG_N = 1 << 2;
constexpr uint8_t SREG_Z = 1 << 1;
constexpr uint8_t SREG_C = 1 << 0;

constexpr uint8_t SREG_HSVNZC = 0x3f;

static ARDENS_FORCEINLINE uint32_t increase_counter(uint32_t& counter, uint32_t inc, uint32_t top)
{
    uint32_t c = counter + inc;
    uint32_t n = c / top;
    counter = c % top;
    return n;
}

template<class T> size_t array_bytes(T const& a)
{
    return a.size() * sizeof(*a.data());
}

}
