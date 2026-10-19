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

void update_spi_via_register(absim::atmega32u4_t& cpu)
{
    // Rewriting SPCR unchanged updates SPI without advancing time or clearing
    // SPIF/WCOL. update_spi() itself is internal to the CPU implementation TU.
    cpu.st(addr::SPCR, cpu.data[addr::SPCR]);
}

int register_boundaries()
{
    int r = 0;
    char const* orders[] = { "handler", "SPCR", "SPSR", "SPDR", "SPSR/SPDR", "queue" };
    for(unsigned order = 0; order < 6; ++order)
    for(unsigned gap : { 16u, 17u, 18u, 41u })
    for(uint8_t requested : { uint8_t(0x96), uint8_t(0) })
    {
        auto a = device({ out(addr::SPDR, 16) });
        auto& cpu = a->core_state.cpu;
        cpu.st(addr::SPDR, 0x03);
        cpu.spi_datain_byte = 0x3c;
        cpu.cycle_count = start + gap;
        if(order == 1) update_spi_via_register(cpu);
        if(order == 2 || order == 4) (void)cpu.ld(addr::SPSR);
        if(order == 3 || order == 4) (void)cpu.ld(addr::SPDR);
        if(order == 5)
        {
            cpu.gpr(16) = requested;
            a->cycle(); // PQ_SPI completes before the OUT handler
        }
        else
            cpu.st(addr::SPDR, requested);

        uint8_t expected_tx = gap == 16 ? 0x03 : gap == 17 ? 0 : requested;
        uint8_t expected_rx = 0x3c;
        bool pass = cpu.spi_busy && cpu.spi_data_byte == expected_tx &&
            cpu.data[addr::SPDR] == expected_rx &&
            bool(cpu.data[addr::SPSR] & bit::SPSR::WCOL) == (gap <= 17) &&
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
    for(unsigned gap : { 15u, 16u, 17u, 18u })
    {
        auto a = device();
        auto& cpu = a->core_state.cpu;
        cpu.st(addr::SPDR, 0x96);
        cpu.spi_datain_byte = 0x3c;
        cpu.cycle_count = start + gap;
        auto rx = cpu.ld(addr::SPDR);
        auto status = cpu.ld(addr::SPSR);
        bool ok = rx == (gap < 16 ? 0x69 : 0x3c) &&
            bool(status & bit::SPSR::SPIF) == (gap >= 18) &&
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
    cpu.spi_datain_byte = 0xa7;
    pass &= cpu.ld(addr::SPDR) == 0x3c; // receive buffer is latched only once
    pass &= (cpu.data[addr::SPSR] & flags) == bit::SPSR::WCOL;
    pass &= (cpu.ld(addr::SPSR) & flags) == bit::SPSR::WCOL;
    pass &= (cpu.data[addr::SPSR] & flags) == bit::SPSR::WCOL;
    pass &= cpu.ld(addr::SPDR) == 0x3c;
    pass &= (cpu.data[addr::SPSR] & flags) == 0;
    // Reading/clearing flags at the boundary must not consume the zero quirk.
    cpu.st(addr::SPDR, 0x96);
    pass &= cpu.spi_data_byte == 0 && cpu.ld(addr::SPDR) == 0x3c;
    pass &= (cpu.data[addr::SPSR] & flags) == bit::SPSR::WCOL;
    cpu.cycle_count = start + 18;
    pass &= (cpu.ld(addr::SPSR) & flags) == flags;
    pass &= cpu.ld(addr::SPDR) == 0x3c && (cpu.data[addr::SPSR] & flags) == 0;
    return result("receive timing and SPSR/SPDR flag clearing", pass);
}

int register_status_flags()
{
    int r = 0;
    for(bool queued : { false, true })
    for(bool write_ack : { false, true })
    {
        auto a = device();
        auto& c = a->core_state.cpu;
        c.st(addr::SPDR, 0x5a);
        c.spi_datain_byte = 0x3c;
        c.cycle_count = start + 17;
        if(queued) a->cycle();
        else update_spi_via_register(c);
        c.cycle_count = start + 18;
        c.st(addr::SPDR, 0x96);
        bool pass = c.spi_busy && c.data[addr::SPDR] == 0x3c &&
            (c.data[addr::SPSR] & flags) == bit::SPSR::SPIF;
        c.cycle_count = start + 19;
        pass &= (c.ld(addr::SPSR) & flags) == bit::SPSR::SPIF;
        c.cycle_count = start + 20;
        if(write_ack) c.st(addr::SPDR, 0xa5);
        else pass &= c.ld(addr::SPDR) == 0x3c;
        uint8_t const acknowledged = c.data[addr::SPSR] & flags;
        pass &= acknowledged == (write_ack ? bit::SPSR::WCOL : 0) &&
            c.spi_busy && c.spi_data_byte == 0x96 &&
            c.spi_transmit_zero_cycle == start + 35 && !c.spsr_read_after_transmit;
        c.cycle_count = start + 36;
        c.spi_datain_byte = 0xa7;
        pass &= c.ld(addr::SPDR) == 0xa7 && !c.spi_busy &&
            (c.data[addr::SPSR] & flags) ==
                (bit::SPSR::SPIF | (write_ack ? bit::SPSR::WCOL : 0));
        printf("      stale SPIF %s/%s ack=%02x next_RX=%02x final=%02x : %s\n",
            queued ? "queue" : "handler", write_ack ? "OUT" : "IN",
            acknowledged, c.data[addr::SPDR], c.data[addr::SPSR], pass ? "PASS" : "FAIL");
        r |= pass ? 0 : 1;
    }
    for(bool write_ack : { false, true })
    {
        auto a = device();
        auto& c = a->core_state.cpu;
        c.st(addr::SPDR, 0x5a);
        c.cycle_count = start + 6;
        c.st(addr::SPDR, 0x96);
        c.cycle_count = start + 7;
        bool pass = (c.ld(addr::SPSR) & flags) == bit::SPSR::WCOL;
        c.cycle_count = start + 8;
        if(write_ack) c.st(addr::SPDR, 0xa5);
        else pass &= c.ld(addr::SPDR) == 0x69;
        pass &= (c.data[addr::SPSR] & flags) == (write_ack ? bit::SPSR::WCOL : 0) &&
            !c.spsr_read_after_transmit && c.spi_busy && c.spi_data_byte == 0x5a &&
            c.spi_transmit_zero_cycle == start + 17;
        r |= result(write_ack ? "WCOL-only OUT acknowledgment/new collision" :
            "WCOL-only IN acknowledgment", pass);
    }
    // A successful write acknowledges an earlier status read; an unrelated
    // successful write must preserve both unacknowledged flags.
    for(bool status_read : { false, true })
    {
        auto a = device();
        auto& c = a->core_state.cpu;
        c.st(addr::SPDR, 0x5a);
        c.cycle_count = start + 6;
        c.st(addr::SPDR, 0x96);
        c.cycle_count = start + 18;
        update_spi_via_register(c);
        if(status_read) (void)c.ld(addr::SPSR);
        c.st(addr::SPDR, 0xa5);
        bool pass = c.spi_busy && c.spi_data_byte == 0xa5 &&
            (c.data[addr::SPSR] & flags) == (status_read ? 0 : flags) &&
            !c.spsr_read_after_transmit;
        r |= result(status_read ? "accepted OUT acknowledges status read" :
            "accepted OUT preserves unacknowledged flags", pass);
    }
    for(unsigned flag_case = 0; flag_case < 4; ++flag_case)
    {
        auto a = device();
        auto& c = a->core_state.cpu;
        c.st(addr::SPDR, 0x5a);
        c.cycle_count = start + 6;
        if(flag_case & 1) c.st(addr::SPDR, 0x96);
        if(flag_case & 2)
        {
            c.cycle_count = start + 18;
            update_spi_via_register(c);
        }
        uint8_t const expected = uint8_t((flag_case & 1 ? bit::SPSR::WCOL : 0) |
            (flag_case & 2 ? bit::SPSR::SPIF : 0));
        bool pass = true;
        for(uint8_t value : { uint8_t(0), uint8_t(1), uint8_t(0xc0), uint8_t(0xff), uint8_t(0x7e) })
        {
            c.st(addr::SPSR, value);
            pass &= c.data[addr::SPSR] == uint8_t(expected | (value & 1)) &&
                c.ld(addr::SPSR) == uint8_t(expected | (value & 1)) &&
                c.spi_clock_cycles == (value & 1 ? 2 : 4);
        }
        printf("      SPSR read-only flags=%02x reserved=%02x : %s\n",
            expected, c.data[addr::SPSR] & 0x3e, pass ? "PASS" : "FAIL");
        r |= pass ? 0 : 1;
    }
    return r | result("register status flag semantics", r == 0);
}

int acknowledgment_across_completion()
{
    int r = 0;
    for(bool spif_read : { false, true })
    for(bool queued : { false, true })
    for(bool write_ack : { false, true })
    {
        auto a = device({ write_ack ? out(addr::SPDR, 17) : in(2, addr::SPDR) });
        auto& c = a->core_state.cpu;
        c.gpr(17) = 0xa5;
        c.st(addr::SPDR, 0x5a);
        if(spif_read)
        {
            c.spi_datain_byte = 0x3c;
            c.cycle_count = start + 17;
            update_spi_via_register(c);
            c.cycle_count = start + 18;
            c.st(addr::SPDR, 0x96);
        }
        else
        {
            c.cycle_count = start + 6;
            c.st(addr::SPDR, 0x96);
        }
        c.cycle_count = start + (spif_read ? 19 : 7);
        bool pass = (c.ld(addr::SPSR) & flags) == (spif_read ? bit::SPSR::SPIF : bit::SPSR::WCOL);
        c.cycle_count = start + (spif_read ? 36 : 18);
        c.spi_datain_byte = spif_read ? 0xa7 : 0x3c;
        if(queued) a->cycle();
        else if(write_ack) c.st(addr::SPDR, 0xa5);
        else (void)c.ld(addr::SPDR);
        // Hardware rows 02/05: old SPIF acknowledgment clears SPIF even
        // after another completion. Rows 08/0B: WCOL-only acknowledgment
        // clears WCOL while preserving the newly raised SPIF.
        pass &= (c.data[addr::SPSR] & flags) == (spif_read ? 0 : bit::SPSR::SPIF) &&
            c.data[addr::SPDR] == (spif_read ? 0xa7 : 0x3c) &&
            c.spi_busy == write_ack && (!write_ack || c.spi_data_byte == 0xa5);
        printf("      completion between %s/%s via %s RX=%02x SPSR=%02x : %s\n",
            spif_read ? "SPIF" : "WCOL", write_ack ? "OUT" : "IN", queued ? "queue" : "handler",
            c.data[addr::SPDR], c.data[addr::SPSR], pass ? "PASS" : "FAIL");
        r |= pass ? 0 : 1;
    }
    return r | result("acknowledgment survives intervening completion", r == 0);
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

int hardware_status_matrix(execution mode)
{
    // Physical FX-C version-01 photographs, transcribed in
    // spi_status_probe/hardware-results.txt. Check status verbatim; the bus
    // fixture supplies distinct flash bytes instead of deselected MISO=FF.
    uint8_t const expected[18][3] = {
        {0x81,0x01,0x81}, {0x81,0x81,0x81}, {0x81,0x01,0x01},
        {0x81,0x41,0xc1}, {0x81,0xc1,0xc1}, {0x81,0x01,0x81},
        {0x41,0x01,0x81}, {0x41,0x81,0x81}, {0x41,0x81,0x81},
        {0x41,0x41,0xc1}, {0x41,0xc1,0xc1}, {0x41,0x81,0x81},
        {0x01,0x01,0x81}, {0x01,0x81,0x81}, {0x01,0x81,0x81},
        {0x01,0x41,0xc1}, {0x01,0xc1,0xc1}, {0x01,0x81,0x81},
    };
    bool pass = true;
    for(unsigned id = 0; id < 18; ++id)
    {
        unsigned const kind = id / 6;
        bool const write = (id % 6) >= 3;
        unsigned const gap = 16 + id % 3;
        unsigned const current = kind == 0 ? 18 : 0;
        unsigned const access = current + gap;
        std::vector<uint16_t> words(access + 36, 0);
        words[0] = out(addr::SPDR, 16);
        if(kind == 0) words[18] = out(addr::SPDR, 17);
        if(kind == 1) words[6] = out(addr::SPDR, 17);
        words[kind == 0 ? 19 : 7] = in(2, addr::SPSR);
        words[access] = write ? out(addr::SPDR, 18) : in(5, addr::SPDR);
        words[access + 1] = in(3, addr::SPSR);
        words[access + 34] = in(4, addr::SPSR);
        words[access + 35] = in(6, addr::SPDR);
        finish_program(words);
        auto a = device(words);
        connect_peripherals(*a);
        auto& c = a->core_state.cpu;
        c.gpr(16) = 0x5a; c.gpr(17) = 0x96; c.gpr(18) = 0xa5;
        run(*a, mode, start + words.size() + 20);
        bool const ok = c.gpr(2) == expected[id][0] &&
            c.gpr(3) == expected[id][1] && c.gpr(4) == expected[id][2];
        if(!ok) printf("      %s hardware status row %02x: %02x %02x %02x (expected %02x %02x %02x)\n",
            mode_name(mode), id, c.gpr(2), c.gpr(3), c.gpr(4),
            expected[id][0], expected[id][1], expected[id][2]);
        pass &= ok;
    }
    printf("      %-19s physical status matrix (18 rows) : %s\n", mode_name(mode), pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int hardware_receive_matrix(execution mode)
{
    // Version-02 hardware RX EF,40,18 is normalized to the fixture's
    // 3C,A7,52. This isolates CPU timing from the flash's JEDEC-ID emulation.
    uint8_t const expected[9][5] = {
        {0x3c,0x3c,0xa7,0xc1,0x01}, // OUT gap 16: ignored
        {0x3c,0xa7,0x52,0xc1,0x01}, // OUT gap 17: zero byte, WCOL
        {0x3c,0xa7,0x52,0x81,0x01}, // OUT gap 18
        {0x3c,0xa7,0x52,0x81,0x01}, // OUT gap 19
        {0x3c,0x3c,0xa7,0x01,0x81}, // IN at 16, status at 17
        {0x3c,0x3c,0xa7,0x81,0x01}, // IN at 17, status at 18
        {0x3c,0x3c,0xa7,0x81,0x01}, // IN at 18
        {0x3c,0xa7,0x52,0xc1,0xc1}, // four OUTs, gap 17
        {0x3c,0xa7,0x52,0x81,0x81}, // four OUTs, gap 18
    };
    bool pass = true;
    for(unsigned row = 0; row < 9; ++row)
    {
        std::vector<uint16_t> words(100, 0);
        words[0] = out(addr::SPDR, 16);
        if(row < 7)
        {
            bool const write = row < 4;
            unsigned const gap = 16 + (write ? row : row - 4);
            unsigned const first = gap + (write ? 1 : 0);
            if(write) words[gap] = out(addr::SPDR, 17);
            words[first] = in(2, addr::SPDR);
            words[first + 1] = in(5, addr::SPSR);
            words[first + 42] = in(3, addr::SPDR);
            words[first + 43] = in(6, addr::SPSR);
            words[first + 44] = out(addr::SPDR, 17);
            words[first + 62] = in(7, addr::SPSR);
            words[first + 63] = in(4, addr::SPDR);
        }
        else
        {
            unsigned const gap = row == 7 ? 17 : 18;
            for(unsigned n = 1; n < 4; ++n)
            {
                words[n * gap] = out(addr::SPDR, 17);
                words[n * gap + 1] = in(uint8_t(n + 1), addr::SPDR);
            }
            words[3 * gap + 2] = in(5, addr::SPSR);
            words[3 * gap + 43] = in(6, addr::SPSR);
        }
        finish_program(words);
        auto a = device(words);
        connect_peripherals(*a);
        auto& c = a->core_state.cpu;
        c.gpr(16) = 0x5a; c.gpr(17) = 0;
        run(*a, mode, start + words.size() + 20);
        bool ok = true;
        for(unsigned field = 0; field < 5; ++field)
            ok &= c.gpr(uint8_t(2 + field)) == expected[row][field];
        if(!ok) printf("      %s hardware receive row %02x: %02x %02x %02x %02x %02x\n",
            mode_name(mode), row + 1, c.gpr(2), c.gpr(3), c.gpr(4), c.gpr(5), c.gpr(6));
        pass &= ok;
    }
    printf("      %-19s physical receive matrix (9 rows) : %s\n", mode_name(mode), pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
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
            (cpu.gpr(4) & flags) == (gap <= 17 ? flags : bit::SPSR::SPIF) &&
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
    bool pass = cpu.gpr(8) == 0x3c && cpu.gpr(2) == 0x3c &&
        cpu.gpr(3) == 0xa7 && cpu.gpr(4) == 0x52 && cpu.gpr(5) == 0xe1 &&
        a->peripherals.display.data_col == 4 &&
        a->peripherals.display.ram[0] == 0x5a &&
        a->peripherals.display.ram[1] == 0 &&
        a->peripherals.display.ram[2] == 0 &&
        a->peripherals.display.ram[3] == 0 &&
        (cpu.data[addr::SPSR] & bit::SPSR::WCOL);
    printf("      %-19s streaming OUT=100,117,134,151 RX=%02x,%02x,%02x,%02x old=%02x : %s\n",
        mode_name(mode), cpu.gpr(2), cpu.gpr(3), cpu.gpr(4), cpu.gpr(5),
        cpu.gpr(8), pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int instruction_receives(execution mode)
{
    std::vector<uint16_t> words(21, 0);
    words[0] = out(addr::SPDR, 16);
    words[15] = in(7, addr::SPDR);
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
    bool pass = cpu.gpr(7) == 0x69 && cpu.gpr(2) == 0x3c && cpu.gpr(3) == 0x3c &&
        cpu.gpr(4) == 0x3c && cpu.gpr(6) == 0x3c &&
        (cpu.gpr(5) & flags) == bit::SPSR::SPIF &&
        (cpu.data[addr::SPSR] & flags) == 0 &&
        a->peripherals.display.data_col == 1 && a->peripherals.display.ram[0] == 0x5a;
    printf("      %-19s IN cycles=116,117,118 RX=%02x,%02x,%02x SPSR=%02x cleared=%02x : %s\n",
        mode_name(mode), cpu.gpr(2), cpu.gpr(3), cpu.gpr(4),
        cpu.gpr(5), cpu.data[addr::SPSR], pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int instruction_status_flags(execution mode)
{
    int r = 0;
    for(bool stale : { false, true })
    for(bool write_ack : { false, true })
    {
        unsigned const offset = stale ? 12 : 0;
        std::vector<uint16_t> words(42, 0);
        words[0] = out(addr::SPDR, 16);
        words[6 + offset] = out(addr::SPDR, 17);
        words[7 + offset] = in(2, addr::SPSR);
        words[8 + offset] = write_ack ? out(addr::SPDR, 18) : in(3, addr::SPDR);
        words[9 + offset] = in(4, addr::SPSR);
        words[10 + offset] = in(7, addr::SPDR); // acknowledge a new WCOL, if any
        words[11 + offset] = in(8, addr::SPSR);
        words[36] = in(5, addr::SPDR);
        words[37] = in(6, addr::SPSR);
        finish_program(words);
        auto a = device(words);
        connect_peripherals(*a);
        auto& c = a->core_state.cpu;
        c.gpr(16) = 0x5a;
        c.gpr(17) = 0x96;
        c.gpr(18) = 0xa5;
        run(*a, mode, start + words.size() + 20);
        bool pass = (c.gpr(2) & flags) == (stale ? bit::SPSR::SPIF : bit::SPSR::WCOL) &&
            (write_ack || c.gpr(3) == (stale ? 0x3c : 0x69)) &&
            (c.gpr(4) & flags) == (write_ack ? bit::SPSR::WCOL : 0) &&
            (c.gpr(8) & flags) == 0 && c.gpr(5) == (stale ? 0xa7 : 0x3c) &&
            (c.gpr(6) & flags) == bit::SPSR::SPIF &&
            a->peripherals.display.data_col == (stale ? 2 : 1) &&
            a->peripherals.display.ram[0] == 0x5a &&
            (!stale || a->peripherals.display.ram[1] == 0x96);
        printf("      %-19s %s/%s status=%02x ack=%02x next=%02x RX=%02x : %s\n",
            mode_name(mode), stale ? "stale SPIF" : "WCOL only", write_ack ? "OUT" : "IN",
            c.gpr(2), c.gpr(4), c.gpr(6), c.gpr(5), pass ? "PASS" : "FAIL");
        r |= pass ? 0 : 1;
    }
    {
        std::vector<uint16_t> words(46, 0);
        words[0] = out(addr::SPDR, 16);
        words[6] = out(addr::SPDR, 17); // WCOL, followed by SPIF at 18
        words[18] = out(addr::SPSR, 19); // cannot clear either flag
        words[19] = in(2, addr::SPSR);
        words[20] = out(addr::SPDR, 17); // accepted OUT acknowledges both flags
        words[21] = in(3, addr::SPSR);
        words[38] = out(addr::SPSR, 18); // after completion: SPI2X=0, preserve SPIF
        words[39] = in(4, addr::SPSR);
        words[40] = out(addr::SPSR, 19); // SPI2X=1, preserve pending acknowledgment
        words[41] = in(5, addr::SPSR);
        words[42] = in(6, addr::SPDR);
        words[43] = in(7, addr::SPSR);
        finish_program(words);
        auto a = device(words);
        connect_peripherals(*a);
        auto& c = a->core_state.cpu;
        c.gpr(16) = 0x5a;
        c.gpr(17) = 0x96;
        c.gpr(18) = 0x7e;
        c.gpr(19) = 0xff;
        run(*a, mode, start + words.size() + 20);
        bool pass = c.gpr(2) == 0xc1 && c.gpr(3) == 1 &&
            c.gpr(4) == 0x80 && c.gpr(5) == 0x81 && c.gpr(6) == 0xa7 && c.gpr(7) == 1 &&
            a->peripherals.display.data_col == 2 && a->peripherals.display.ram[1] == 0x96;
        printf("      %-19s OUT acknowledgment/SPSR writes=%02x,%02x,%02x,%02x,%02x : %s\n",
            mode_name(mode), c.gpr(2), c.gpr(3), c.gpr(4), c.gpr(5), c.gpr(7), pass ? "PASS" : "FAIL");
        r |= pass ? 0 : 1;
    }
    // An early OLED acknowledgment leaves SPIF set at cycle 18. A later
    // IN/SBRS/RJMP poll must expose the stale flag, premature RX and WCOL.
    for(bool acknowledge_first : { false, true })
    {
        std::vector<uint16_t> words(64, 0);
        words[0] = out(addr::SPDR, 16);
        words[15] = in(10, addr::SPSR);
        words[16] = in(11, addr::SPDR);
        unsigned const poll = acknowledge_first ? 21 : 19;
        if(acknowledge_first)
        {
            words[18] = in(12, addr::SPSR);
            words[19] = in(13, addr::SPDR);
        }
        words[poll - 1] = out(addr::SPDR, 17);
        words[poll] = in(20, addr::SPSR);
        words[poll + 1] = 0xff47; // SBRS r20, 7
        words[poll + 2] = 0xcffd; // RJMP back to IN
        words[poll + 3] = in(3, addr::SPDR);
        words[poll + 4] = out(addr::SPDR, 18);
        words[60] = in(5, addr::SPDR);
        words[61] = in(6, addr::SPSR);
        finish_program(words);
        auto a = device(words);
        connect_peripherals(*a);
        auto& c = a->core_state.cpu;
        c.gpr(16) = 0x5a;
        c.gpr(17) = 0x96;
        c.gpr(18) = 0xa5;
        run(*a, mode, start + words.size() + 20);
        bool pass = (c.gpr(10) & flags) == 0 && c.gpr(11) == 0x3c &&
            c.gpr(3) == (acknowledge_first ? 0xa7 : 0x3c) &&
            c.gpr(5) == (acknowledge_first ? 0x52 : 0xa7) &&
            (c.gpr(6) & flags) == (acknowledge_first ? bit::SPSR::SPIF : flags) &&
            a->peripherals.display.data_col == (acknowledge_first ? 3 : 2) &&
            a->peripherals.display.ram[1] == 0x96 &&
            (!acknowledge_first || a->peripherals.display.ram[2] == 0xa5);
        printf("      %-19s %s poll RX=%02x final_RX=%02x SPSR=%02x : %s\n",
            mode_name(mode), acknowledge_first ? "acknowledged" : "stale", c.gpr(3),
            c.gpr(5), c.gpr(6), pass ? "PASS" : "FAIL");
        r |= pass ? 0 : 1;
    }
    return r;
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
        pass &= cpu.ld(addr::SPDR) == (dividers[setting] == 2 ? 0x3c : 0x69) && cpu.spi_busy;
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
    for(unsigned phase = 0; phase < 5; ++phase)
    {
        std::vector<uint16_t> words(40, 0);
        finish_program(words);
        auto a = device(words);
        auto& cpu = a->core_state.cpu;
        connect_peripherals(*a);
        cpu.st(addr::SPDR, 0x96);
        // Before delivery, after delivery, after RX latching, after becoming
        // writable, and after an OUT at 17 while the old SPIF is pending at 18.
        run(*a, execution::ordinary, start + (phase == 0 ? 15 : phase < 3 ? 16 : 17));
        if(phase >= 2) update_spi_via_register(cpu);
        if(phase == 4) cpu.st(addr::SPDR, 0xa5);
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
            if(phase < 4) c.st(addr::SPDR, 0xa5);
            ok &= c.spi_busy && c.spi_data_byte == 0 &&
                c.data[addr::SPDR] == 0x3c &&
                c.spi_transmit_zero_cycle == start + 34 && c.spi_flag_cycle == start + 18 &&
                (c.data[addr::SPSR] & flags) == bit::SPSR::WCOL;
            run(*d, execution::ordinary, start + 18);
            update_spi_via_register(c);
            ok &= c.spi_busy && (c.data[addr::SPSR] & flags) == flags;
            run(*d, mode, start + 35);
            update_spi_via_register(c);
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
            cpu.spi_transmit_zero_cycle == UINT64_MAX && cpu.spi_flag_cycle == UINT64_MAX &&
            !cpu.spi_data_latched && !cpu.spi_fast_transfer;
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

int status_snapshot_and_reset()
{
    int r = 0;
    for(auto mode : { execution::ordinary, execution::merged })
    for(bool stale : { false, true })
    for(bool write_ack : { false, true })
    {
        std::vector<uint16_t> words(64, 0);
        finish_program(words);
        auto a = device(words);
        connect_peripherals(*a);
        auto& c = a->core_state.cpu;
        c.st(addr::SPDR, 0x5a);
        run(*a, execution::ordinary, start + (stale ? 18 : 6));
        c.st(addr::SPDR, 0x96); // accepted next transfer or first-transfer collision
        run(*a, execution::ordinary, start + (stale ? 19 : 7));
        bool pass = (c.ld(addr::SPSR) & flags) == (stale ? bit::SPSR::SPIF : bit::SPSR::WCOL) &&
            (stale ? c.spsr_read_after_transmit : c.spi_wcol_read);
        std::ostringstream saved;
        pass &= a->save_snapshot(saved);
        auto b = std::make_unique<device_t>();
        std::istringstream input(saved.str());
        auto err = b->load_snapshot(input);
        pass &= err.empty();
        if(!err.empty())
        {
            r |= result("status snapshot load", false);
            continue;
        }
        b->reset();
        auto& reset_cpu = b->core_state.cpu;
        pass &= !reset_cpu.spsr_read_after_transmit && !reset_cpu.spi_wcol_read && !reset_cpu.spi_busy &&
            reset_cpu.data[addr::SPSR] == 0 && reset_cpu.spi_transmit_zero_cycle == UINT64_MAX;
        std::istringstream again(saved.str());
        pass &= b->load_snapshot(again).empty();
        for(auto* d : { a.get(), b.get() })
        {
            auto& cpu = d->core_state.cpu;
            pass &= stale ? cpu.spsr_read_after_transmit : cpu.spi_wcol_read;
            run(*d, execution::ordinary, start + (stale ? 20 : 8));
            if(write_ack) cpu.st(addr::SPDR, 0xa5);
            else pass &= cpu.ld(addr::SPDR) == (stale ? 0x3c : 0x69);
            pass &= !cpu.spsr_read_after_transmit && !cpu.spi_wcol_read &&
                (cpu.data[addr::SPSR] & flags) == (write_ack ? bit::SPSR::WCOL : 0);
            run(*d, mode, start + (stale ? 37 : 19));
            pass &= cpu.ld(addr::SPDR) == (stale ? 0xa7 : 0x3c) &&
                (cpu.data[addr::SPSR] & flags) ==
                    (bit::SPSR::SPIF | (write_ack ? bit::SPSR::WCOL : 0));
        }
        std::ostringstream after_a, after_b;
        pass &= a->save_snapshot(after_a) && b->save_snapshot(after_b) && after_a.str() == after_b.str();
        printf("      %-19s pending %s/%s snapshot RX=%02x SPSR=%02x : %s\n",
            mode_name(mode), stale ? "SPIF" : "WCOL", write_ack ? "OUT" : "IN",
            c.data[addr::SPDR], c.data[addr::SPSR], pass ? "PASS" : "FAIL");
        r |= pass ? 0 : 1;
    }
    return r | result("status acknowledgment snapshot/reset replay", r == 0);
}

} // namespace

int spi_tests()
{
    printf("\nSPI timing regression tests...\n");
    int r = register_boundaries();
    r |= receive_and_flags();
    r |= register_status_flags();
    r |= acknowledgment_across_completion();
    for(auto mode : { execution::ordinary, execution::merged, execution::step,
        execution::continuous, execution::continuous_unmerged })
    {
        r |= instruction_boundaries(mode);
        r |= instruction_receives(mode);
        r |= streaming(mode);
        r |= instruction_status_flags(mode);
        r |= hardware_status_matrix(mode);
        r |= hardware_receive_matrix(mode);
    }
    r |= clock_and_mode_controls();
    r |= snapshot_and_reset();
    r |= status_snapshot_and_reset();
    return r | result("all timing regressions", r == 0);
}
