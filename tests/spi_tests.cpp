#include <absim.hpp>

#include <cstdio>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace absim::reg;
using device_t = absim::arduboy_t;
constexpr uint8_t flags = bit::SPSR::SPIF | bit::SPSR::WCOL;
constexpr uint64_t start = 100;

// Encode real AVR instructions so decode, merging, I/O barriers and the
// peripheral queue all participate. No assembler, bootloader or ROM required.
uint16_t out(uint8_t reg, uint8_t source)
{
    unsigned io = reg - 0x20;
    return uint16_t(0xb800 | ((io & 0x30) << 5) | (source << 4) | (io & 15));
}

uint16_t in(uint8_t dest, uint8_t reg)
{
    return uint16_t(out(reg, dest) ^ 0x0800);
}

std::unique_ptr<device_t> device(std::vector<uint16_t> const& words = {})
{
    auto a = std::make_unique<device_t>();
    a->program_state.cfg.bootloader = false;
    std::string hex;
    for(size_t i = 0; i < (words.empty() ? 1 : words.size()); ++i)
    {
        unsigned word = words.empty() ? 0 : words[i];
        unsigned address = unsigned(i * 2);
        unsigned lo = word & 255, hi = word >> 8;
        unsigned checksum = (0u - (2 + (address >> 8) + (address & 255) + lo + hi)) & 255;
        char line[32];
        snprintf(line, sizeof(line), ":02%04X00%02X%02X%02X\n", address, lo, hi, checksum);
        hex += line;
    }
    hex += ":00000001FF\n";
    std::istringstream input(hex);
    auto err = a->load_file("spi.hex", input);
    if(!err.empty()) throw std::runtime_error(err);
    a->reset();
    auto& cpu = a->core_state.cpu;
    cpu.cycle_count = start;
    cpu.pc = 0;
    cpu.no_merged = true;
    cpu.st(addr::SPCR, 0x50); // SPE | MSTR, mode 0
    cpu.st(addr::SPSR, 0x01); // F_CPU / 2
    cpu.data[addr::SPDR] = 0x69; // distinct old receive-buffer contents
    return a;
}

int result(char const* name, bool pass)
{
    printf("   SPI %-48s : %s\n", name, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int register_boundaries()
{
    int r = 0;
    char const* orders[] = { "handler", "update", "SPSR", "SPDR", "SPSR/SPDR", "queue" };
    for(unsigned order = 0; order < 6; ++order)
    for(unsigned gap : { 16u, 17u, 18u, 41u })
    for(uint8_t requested : { uint8_t(0x96), uint8_t(0) })
    {
        auto a = device({ out(addr::SPDR, 16) });
        auto& cpu = a->core_state.cpu;
        cpu.st(addr::SPDR, 0x03);
        cpu.spi_datain_byte = 0x3c;
        cpu.cycle_count = start + gap;
        if(order == 1) cpu.update_spi();
        if(order == 2 || order == 4) (void)cpu.ld(addr::SPSR);
        if(order == 3 || order == 4) (void)cpu.ld(addr::SPDR);
        if(order == 5)
        {
            cpu.gpr(16) = requested;
            cpu.advance_cycle(); // PQ_SPI completes before the OUT handler
        }
        else
            cpu.st(addr::SPDR, requested);

        uint8_t expected_tx = gap == 16 ? 0x03 : gap == 17 ? 0 : requested;
        uint8_t expected_rx = gap == 16 ? 0x69 : 0x3c;
        bool pass = cpu.spi_busy && cpu.spi_data_byte == expected_tx &&
            cpu.data[addr::SPDR] == expected_rx &&
            bool(cpu.data[addr::SPSR] & bit::SPSR::WCOL) == (gap == 16) &&
            cpu.spi_transmit_zero_cycle == (gap == 16 ? start + 17 : start + gap + 17);
        printf("      %-9s cycle=%llu gap=%u requested=%02x tx=%02x rx=%02x SPSR=%02x : %s\n",
            orders[order], (unsigned long long)(start + gap), gap, requested,
            cpu.spi_data_byte, cpu.data[addr::SPDR], cpu.data[addr::SPSR],
            pass ? "PASS" : "FAIL");
        r |= pass ? 0 : 1;
    }
    return r | result("register write boundaries", r == 0);
}

int receive_and_flags()
{
    bool pass = true;
    for(unsigned gap : { 16u, 17u, 18u })
    {
        auto a = device();
        auto& cpu = a->core_state.cpu;
        cpu.st(addr::SPDR, 0x96);
        cpu.spi_datain_byte = 0x3c;
        cpu.cycle_count = start + gap;
        auto rx = cpu.ld(addr::SPDR);
        auto status = cpu.ld(addr::SPSR);
        bool ok = rx == (gap == 16 ? 0x69 : 0x3c) &&
            bool(status & bit::SPSR::SPIF) == (gap >= 17) &&
            cpu.spi_busy == (gap < 17);
        pass &= ok;
        printf("      read cycle=%llu rx=%02x SPSR=%02x : %s\n",
            (unsigned long long)cpu.cycle_count, rx, status, ok ? "PASS" : "FAIL");
    }

    auto a = device();
    auto& cpu = a->core_state.cpu;
    cpu.st(addr::SPDR, 0x96);
    cpu.spi_datain_byte = 0x3c;
    cpu.cycle_count = start + 16;
    cpu.st(addr::SPDR, 0xa5); // collision must leave the first transfer intact
    pass &= (cpu.data[addr::SPSR] & flags) == bit::SPSR::WCOL;
    cpu.cycle_count = start + 17;
    pass &= cpu.ld(addr::SPDR) == 0x3c; // SPDR alone does not clear flags
    pass &= (cpu.data[addr::SPSR] & flags) == flags;
    pass &= (cpu.ld(addr::SPSR) & flags) == flags;
    pass &= (cpu.data[addr::SPSR] & flags) == flags; // SPSR alone also preserves them
    pass &= cpu.ld(addr::SPDR) == 0x3c;
    pass &= (cpu.data[addr::SPSR] & flags) == 0;
    // Reading/clearing flags at the boundary must not consume the zero quirk.
    cpu.st(addr::SPDR, 0x96);
    pass &= cpu.spi_data_byte == 0 && cpu.ld(addr::SPDR) == 0x3c;
    return result("receive timing and SPSR/SPDR flag clearing", pass);
}

enum class execution { ordinary, merged, step, continuous, continuous_unmerged };
char const* mode_name(execution mode)
{
    switch(mode)
    {
    case execution::ordinary: return "ordinary";
    case execution::merged: return "merged";
    case execution::step: return "step";
    case execution::continuous: return "continuous";
    default: return "continuous-unmerged";
    }
}

void run(device_t& a, execution mode, uint64_t end)
{
    a.core_state.cpu.no_merged = mode != execution::merged;
    if(mode == execution::continuous || mode == execution::continuous_unmerged)
    {
        // An enabled breakpoint outside this program selects no_merged in
        // advance(), just as normal execution under the debugger does.
        a.debugger_state.allow_nonstep_breakpoints = mode == execution::continuous_unmerged;
        a.debugger_state.breakpoints.set(0x3fff);
        a.advance(device_t::PS_BUFFER + (end - a.core_state.cpu.cycle_count) * device_t::CYCLE_PS);
    }
    else
        while(a.core_state.cpu.cycle_count < end)
        {
            if(mode == execution::step) a.advance_instr();
            else a.cycle();
        }
}

void connect_peripherals(device_t& a)
{
    // Observe actual OLED bytes and receive distinct bytes from the flash.
    a.core_state.cpu.data[addr::PORTD] = bit::PORTD::PORTD7 | bit::PORTD::PORTD4;
    a.peripherals.prev_display_reset = false;
    a.peripherals.fx.woken_up = true;
    a.peripherals.fx.reading = 4;
    a.peripherals.fx.current_addr = 0x1234;
    uint8_t const bytes[] = { 0x3c, 0xa7, 0x52, 0xe1 };
    a.peripherals.fx.write_bytes(0x1234, bytes, sizeof(bytes));
}

void finish_program(std::vector<uint16_t>& words)
{
    // Stop I/O, but allow all pending transfers to complete in every runner.
    words.push_back(0xcfff); // RJMP .
}

int instruction_boundaries(execution mode)
{
    int r = 0;
    for(unsigned gap : { 16u, 17u, 18u, 25u })
    for(uint8_t requested : { uint8_t(0x96), uint8_t(0) })
    {
        std::vector<uint16_t> words(gap + 20, 0);
        words[0] = out(addr::SPDR, 16);
        words[gap] = out(addr::SPDR, 17);
        words[gap + 1] = in(2, addr::SPDR); // receive the first byte after OUT
        words[gap + 18] = in(3, addr::SPDR);
        words[gap + 19] = in(4, addr::SPSR);
        finish_program(words);
        auto a = device(words);
        auto& cpu = a->core_state.cpu;
        connect_peripherals(*a);
        cpu.gpr(16) = 0x5a;
        cpu.gpr(17) = requested;
        run(*a, mode, start + words.size() + 20);
        auto const& display = a->peripherals.display;
        uint8_t expected_tx = gap == 17 ? 0 : requested;
        bool pass = display.ram[0] == 0x5a &&
            display.data_col == (gap == 16 ? 1 : 2) &&
            (gap == 16 || display.ram[1] == expected_tx) &&
            cpu.gpr(2) == 0x3c && cpu.gpr(3) == (gap == 16 ? 0x3c : 0xa7) &&
            (cpu.gpr(4) & flags) == (gap == 16 ? flags : bit::SPSR::SPIF) &&
            a->peripherals.fx.current_addr == (gap == 16 ? 0x1235 : 0x1236);
        printf("      %-19s OUT cycles=%llu,%llu OLED=%02x,%02x count=%u RX=%02x,%02x SPSR=%02x : %s\n",
            mode_name(mode), (unsigned long long)start, (unsigned long long)(start + gap),
            display.ram[0], display.ram[1], unsigned(display.data_col),
            cpu.gpr(2), cpu.gpr(3), cpu.gpr(4), pass ? "PASS" : "FAIL");
        r |= pass ? 0 : 1;
    }
    return r;
}

int streaming(execution mode)
{
    std::vector<uint16_t> words(72, 0);
    // OUT at 17 / IN at 18, repeated with three distinct flash responses.
    for(unsigned n = 0; n < 4; ++n)
    {
        words[n * 17] = out(addr::SPDR, uint8_t(16 + n));
        if(n != 0) words[n * 17 + 1] = in(uint8_t(n + 1), addr::SPDR);
    }
    words[16] = in(8, addr::SPDR);
    words[69] = in(5, addr::SPDR);
    finish_program(words);
    auto a = device(words);
    auto& cpu = a->core_state.cpu;
    connect_peripherals(*a);
    cpu.gpr(16) = 0x5a;
    cpu.gpr(17) = 0;
    cpu.gpr(18) = 0x96;
    cpu.gpr(19) = 0xa5;
    run(*a, mode, start + words.size() + 20);
    bool pass = cpu.gpr(8) == 0x69 && cpu.gpr(2) == 0x3c &&
        cpu.gpr(3) == 0xa7 && cpu.gpr(4) == 0x52 && cpu.gpr(5) == 0xe1 &&
        a->peripherals.display.data_col == 4 &&
        a->peripherals.display.ram[0] == 0x5a &&
        a->peripherals.display.ram[1] == 0 &&
        a->peripherals.display.ram[2] == 0 &&
        a->peripherals.display.ram[3] == 0 &&
        !(cpu.data[addr::SPSR] & bit::SPSR::WCOL);
    printf("      %-19s streaming OUT=100,117,134,151 RX=%02x,%02x,%02x,%02x old=%02x : %s\n",
        mode_name(mode), cpu.gpr(2), cpu.gpr(3), cpu.gpr(4), cpu.gpr(5),
        cpu.gpr(8), pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int instruction_receives(execution mode)
{
    std::vector<uint16_t> words(21, 0);
    words[0] = out(addr::SPDR, 16);
    words[16] = in(2, addr::SPDR);
    words[17] = in(3, addr::SPDR);
    words[18] = in(4, addr::SPDR);
    words[19] = in(5, addr::SPSR);
    words[20] = in(6, addr::SPDR);
    finish_program(words);
    auto a = device(words);
    auto& cpu = a->core_state.cpu;
    connect_peripherals(*a);
    cpu.gpr(16) = 0x5a;
    run(*a, mode, start + words.size() + 20);
    bool pass = cpu.gpr(2) == 0x69 && cpu.gpr(3) == 0x3c &&
        cpu.gpr(4) == 0x3c && cpu.gpr(6) == 0x3c &&
        (cpu.gpr(5) & flags) == bit::SPSR::SPIF &&
        (cpu.data[addr::SPSR] & flags) == 0 &&
        a->peripherals.display.data_col == 1 && a->peripherals.display.ram[0] == 0x5a;
    printf("      %-19s IN cycles=116,117,118 RX=%02x,%02x,%02x SPSR=%02x cleared=%02x : %s\n",
        mode_name(mode), cpu.gpr(2), cpu.gpr(3), cpu.gpr(4),
        cpu.gpr(5), cpu.data[addr::SPSR], pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int clock_and_mode_controls()
{
    bool pass = true;
    unsigned const dividers[] = { 4, 16, 64, 128, 2, 8, 32, 64 };
    for(unsigned setting = 0; setting < 8; ++setting)
    for(unsigned mode = 0; mode < 4; ++mode)
    {
        auto a = device();
        auto& cpu = a->core_state.cpu;
        cpu.st(addr::SPCR, uint8_t(0x50 | (mode << 2) | (setting & 3)));
        cpu.st(addr::SPSR, uint8_t(setting >> 2));
        cpu.st(addr::SPDR, 0x96);
        cpu.spi_datain_byte = 0x3c;
        uint64_t boundary = start + dividers[setting] * 8 + 1;
        pass &= cpu.spi_clock_cycles == dividers[setting];
        cpu.cycle_count = boundary - 1;
        pass &= cpu.ld(addr::SPDR) == 0x69 && cpu.spi_busy;
        cpu.st(addr::SPDR, 0xa5);
        pass &= (cpu.data[addr::SPSR] & flags) == bit::SPSR::WCOL && cpu.spi_data_byte == 0x96;
        cpu.cycle_count = boundary;
        pass &= cpu.ld(addr::SPDR) == 0x3c && !cpu.spi_busy;
        cpu.cycle_count = boundary + 1;
        cpu.st(addr::SPDR, 0xa5);
        pass &= cpu.spi_data_byte == 0xa5 && cpu.data[addr::SPDR] == 0x3c;
    }
    auto a = device();
    auto& cpu = a->core_state.cpu;
    cpu.st(addr::SPCR, 0x70); // DORD
    cpu.st(addr::SPDR, 0x96);
    pass &= cpu.spi_data_byte == 0x69;
    cpu.cycle_count = start + 18;
    cpu.st(addr::SPDR, 0x96);
    pass &= cpu.spi_data_byte == 0x69;
    return result("clock dividers, modes and bit-order controls", pass);
}

int snapshot_and_reset()
{
    bool pass = true;
    for(auto mode : { execution::ordinary, execution::merged })
    for(unsigned phase = 0; phase < 3; ++phase)
    {
        std::vector<uint16_t> words(40, 0);
        finish_program(words);
        auto a = device(words);
        auto& cpu = a->core_state.cpu;
        connect_peripherals(*a);
        cpu.st(addr::SPDR, 0x96);
        // Save before board delivery, after delivery but before CPU latching,
        // and after CPU latching. Land exactly on each boundary before saving.
        run(*a, execution::ordinary, phase == 0 ? start + 16 : start + 17);
        if(phase == 2) cpu.update_spi();
        std::ostringstream saved;
        bool ok = a->save_snapshot(saved);
        auto b = std::make_unique<device_t>();
        std::istringstream input(saved.str());
        auto err = b->load_snapshot(input);
        ok &= err.empty();
        if(!err.empty())
        {
            printf("      snapshot phase=%u: %s\n", phase, err.c_str());
            pass = false;
            continue;
        }
        for(auto* d : { a.get(), b.get() })
        {
            auto& c = d->core_state.cpu;
            run(*d, execution::ordinary, start + 17);
            c.st(addr::SPDR, 0xa5);
            ok &= c.spi_busy && c.spi_data_byte == 0 &&
                c.data[addr::SPDR] == 0x3c &&
                c.spi_transmit_zero_cycle == start + 34;
            run(*d, mode, start + 35);
            c.update_spi();
            ok &= !c.spi_busy && c.ld(addr::SPDR) == 0xa7 &&
                d->peripherals.display.data_col == 2 &&
                d->peripherals.display.ram[0] == 0x96 &&
                d->peripherals.display.ram[1] == 0 &&
                d->peripherals.fx.current_addr == 0x1236;
        }
        std::ostringstream after_a, after_b;
        ok &= a->save_snapshot(after_a) && b->save_snapshot(after_b);
        ok &= after_a.str() == after_b.str();
        a->reset();
        ok &= !cpu.spi_busy && cpu.spi_done_cycle == UINT64_MAX &&
            cpu.spi_transmit_zero_cycle == UINT64_MAX;
        cpu.st(addr::SPCR, 0x50);
        cpu.st(addr::SPSR, 1);
        cpu.st(addr::SPDR, 0x96);
        ok &= cpu.spi_data_byte == 0x96;
        printf("      %-19s snapshot phase=%u replay OLED=96,00 RX=3c,a7 reset_tx=%02x : %s\n",
            mode_name(mode), phase, cpu.spi_data_byte, ok ? "PASS" : "FAIL");
        pass &= ok;
    }
    return result("pending/completed snapshot replay and reset", pass);
}

} // namespace

int spi_tests()
{
    printf("\nSPI timing regression tests...\n");
    int r = register_boundaries();
    r |= receive_and_flags();
    for(auto mode : { execution::ordinary, execution::merged, execution::step,
        execution::continuous, execution::continuous_unmerged })
    {
        r |= instruction_boundaries(mode);
        r |= instruction_receives(mode);
        r |= streaming(mode);
    }
    r |= clock_and_mode_controls();
    r |= snapshot_and_reset();
    return r | result("all timing regressions", r == 0);
}
