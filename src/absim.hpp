#pragma once

#include <vector>
#include <array>
#include <bitset>

#include <stdint.h>
#include <assert.h>

#include "absim_instructions.hpp"

namespace absim
{

struct avr_instr_t
{
    uint16_t func;
    uint16_t word;
    uint8_t src;
    uint8_t dst;
};

struct atmega32u4_t
{
    std::array<uint8_t, 2560 + 256> data;

    static constexpr size_t PROG_SIZE_BYTES = 29 * 1024;

    uint8_t& gpr(uint8_t n)
    {
        assert(n < 32);
        return data[n];
    }

    uint16_t just_read;
    uint16_t just_written;

    uint8_t ld(uint16_t ptr)
    {
        just_read = ptr;
        return ptr < data.size() ? data[ptr] : 0x00;
    }
    void st(uint16_t ptr, uint8_t x)
    {
        just_written = ptr;
        if(ptr < data.size()) data[ptr] = x;
    }

    uint8_t ld_ior(uint8_t n)
    {
        return ld(n + 32);
    }
    void st_ior(uint8_t n, uint8_t x)
    {
        st(n + 32, x);
    }

    uint16_t gpr_word(uint8_t n)
    {
        uint16_t lo = gpr(n + 0);
        uint16_t hi = gpr(n + 1);
        return lo | (hi << 8);
    }

    uint16_t w_word() { return gpr_word(24); }
    uint16_t x_word() { return gpr_word(26); }
    uint16_t y_word() { return gpr_word(28); }
    uint16_t z_word() { return gpr_word(30); }

    // false if the cpu is sleeping
    bool active;
    uint8_t wakeup_cycles; // for tracking interrupt wakeup delay

    uint8_t& smcr()   { return data[0x53]; }
    uint8_t& mcucr()  { return data[0x55]; }
    uint8_t& spl()    { return data[0x5d]; }
    uint8_t& sph()    { return data[0x5e]; }
    uint8_t& sreg()   { return data[0x5f]; }

    uint8_t& tifr0()  { return data[0x35]; }
    uint8_t& tifr1()  { return data[0x36]; }
    uint8_t& tifr3()  { return data[0x38]; }
    uint8_t& tifr4()  { return data[0x39]; }

    uint8_t& timsk0() { return data[0x6e]; }
    uint8_t& timsk1() { return data[0x6f]; }
    uint8_t& timsk3() { return data[0x71]; }
    uint8_t& timsk4() { return data[0x72]; }

    uint8_t& tccr0a() { return data[0x44]; }
    uint8_t& tccr0b() { return data[0x45]; }
    uint8_t& tcnt0()  { return data[0x46]; }
    uint8_t& ocr0a()  { return data[0x47]; }
    uint8_t& ocr0b()  { return data[0x48]; }

    uint16_t sp()
    {
        return (uint16_t)spl() | ((uint16_t)sph() << 8);
    }

    void push(uint8_t x)
    {
        uint16_t tsp = sp();
        st(tsp, x);
        --tsp;
        spl() = uint8_t(tsp >> 0);
        sph() = uint8_t(tsp >> 8);
    }

    uint8_t pop()
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

    // SREG after previous instruction (used for interrupt bit)
    uint8_t prev_sreg;

    uint16_t pc;                       // program counter

    uint16_t executing_instr_pc;
    uint8_t cycles_till_next_instr;

    uint16_t last_addr;
    uint16_t num_instrs;
    std::array<avr_instr_t, PROG_SIZE_BYTES / 2> decoded_prog;
    std::array<disassembled_instr_t, PROG_SIZE_BYTES / 2> disassembled_prog;
    bool decoded;
    void decode();

    // timer0
    uint16_t timer0_divider_cycle;
    bool timer0_count_down;
    void cycle_timer0();

    // timer1
    uint16_t timer1_divider_cycle;
    bool timer1_count_down;
    void cycle_timer1();

    // timer3
    uint16_t timer3_divider_cycle;
    bool timer3_count_down;
    void cycle_timer3();

    // PLL
    uint64_t pll_lock_cycle;
    void cycle_pll();

    // SPI
    bool spsr_read_after_transmit;
    bool spi_busy;
    bool spi_done;
    uint8_t spi_data_byte;
    uint8_t spi_clock_cycle;
    uint8_t spi_bit_progress;
    void cycle_spi();

    // EEPROM
    uint8_t eeprom_clear_eempe_cycles;
    uint16_t eeprom_write_addr;
    uint8_t eeprom_write_data;
    uint32_t eeprom_program_cycles;
    void cycle_eeprom();

    bool interrupting;
    void check_interrupt(uint8_t vector, uint8_t flag, uint8_t& tifr);

    // breakpoints
    std::bitset<PROG_SIZE_BYTES / 2> breakpoints;

    uint64_t cycle_count;

    // set all registers to initial value
    void reset();

    // execute one cycle
    void advance_cycle();
};

struct ssd1306_t
{
    // filtered display output (not physically real -- for rendering only)
    std::array<double, 8192> filtered_pixels;
    std::array<uint8_t, 8192> pixels;

    // physical display RAM
    std::array<uint8_t, 1024> ram;

    uint8_t contrast;
    bool entire_display_on;
    bool inverse_display;
    bool display_on;

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

    // for page addressing mode
    uint8_t page_col_start;
    uint8_t page_page_start;

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

    void update_internals();

    // display refresh state
    uint8_t row;
    uint8_t row_cycle;
    uint8_t cycles_per_row;
    uint64_t ps_per_clk;

    void update_pixels_row();
    void filter_pixels(uint64_t ps, double ratio);

    uint64_t ps_rem;

    bool processing_command;
    uint8_t current_command;
    uint8_t command_byte_index;

    // data address state
    uint8_t data_page;
    uint8_t data_col;

    void send_data(uint8_t byte);
    void send_command(uint8_t byte);

    // advance controller state by a given time
    void advance(uint64_t ps);
};

struct arduboy_t
{
    atmega32u4_t cpu;
    ssd1306_t display;

    static constexpr size_t NUM_INSTRS = atmega32u4_t::PROG_SIZE_BYTES / 2;

    std::array<uint64_t, NUM_INSTRS> profiler_counts;
    uint64_t profiler_total;
    uint64_t profiler_total_with_sleep;
    bool profiler_enabled;
    
    struct hotspot_t
    {
        uint64_t count;
        uint16_t begin, end;
    };
    std::array<hotspot_t, NUM_INSTRS> profiler_hotspots;
    uint32_t num_hotspots;

    void profiler_build_hotspots();
    void profiler_reset();

    uint64_t ps_rem;
    uint64_t ps_filter_count; // for calls to display filter

    // paused at breakpoint
    bool paused;

    void reset();

    void cycle();

    void advance_instr();

    // advance by specified number of picoseconds
    // ratio is for display filtering: 1.0 means real time,
    // 0.1 means 10x slower
    void advance(uint64_t ps, double ratio = 1.0);

    // returns an error string on error or nullptr on success
    char const* load_file(char const* filename);
};


constexpr uint8_t SREG_I = 1 << 7;
constexpr uint8_t SREG_T = 1 << 6;
constexpr uint8_t SREG_H = 1 << 5;
constexpr uint8_t SREG_S = 1 << 4;
constexpr uint8_t SREG_V = 1 << 3;
constexpr uint8_t SREG_N = 1 << 2;
constexpr uint8_t SREG_Z = 1 << 1;
constexpr uint8_t SREG_C = 1 << 0;

}
