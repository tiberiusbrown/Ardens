#include <absim.hpp>

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "../src/absim_strstream.hpp"

#define WRITE_IMAGES 0

#if WRITE_IMAGES
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

static const char* snapshot_name = "dazzledash";
static const char* snapshot_rom = "dazzledash.arduboy";

static std::unique_ptr<absim::arduboy_t> arduboy;

static void advance(absim::arduboy_t& a, int ms)
{
    for(int i = 0; i < ms; ++i)
    {
        a.advance(1'000'000'000ull); // 1 ms
        a.core_state.cpu.sound_buffer.clear();
    }
}

static int test(char const* name)
{
    int r = 0;

    std::string fname =
        std::string(TESTS_DIR "/") + name + "/" + name + ".ino-arduboy-fx.hex";
    std::ifstream f(fname);

    auto err = arduboy->load_file("test.hex", f);
    if(!err.empty()) return 1;
    arduboy->reset();
    auto const& d = arduboy->core_state.cpu.serial_bytes;
    for(int i = 0; i < 10000; ++i) // up to ten seconds
    {
        arduboy->advance(1'000'000'000ull); // 1 ms
        if(!d.empty()) break;
        arduboy->core_state.cpu.sound_buffer.clear();
    }

    bool pass = (d.size() == 1 && d[0] == 'P');

    printf("   %-30s : %s\n", name, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

static std::string normalize_line_endings(std::string const& s)
{
    std::string r;
    r.reserve(s.size());
    for(size_t i = 0; i < s.size(); ++i)
    {
        if(s[i] == '\r')
        {
            if(i + 1 < s.size() && s[i + 1] == '\n')
                ++i;
            r.push_back('\n');
        }
        else
        {
            r.push_back(s[i]);
        }
    }
    return r;
}

static std::string serial_bytes_to_string(std::vector<uint8_t> const& bytes)
{
    return std::string(bytes.begin(), bytes.end());
}

static bool serial_has_end(std::vector<uint8_t> const& bytes)
{
    auto output = normalize_line_endings(serial_bytes_to_string(bytes));
    return output == "END\n" ||
        output.size() >= 5 && output.compare(output.size() - 5, 5, "\nEND\n") == 0;
}

static int serial_output_test(char const* name, char const* expected_filename)
{
    std::string hex_name =
        std::string(TESTS_DIR "/") + name + "/" + name + ".ino-arduboy-fx.hex";
    std::ifstream hex(hex_name);
    auto err = arduboy->load_file("test.hex", hex);
    if(!err.empty())
    {
        printf("   %-30s : FAIL\n", name);
        return 1;
    }

    std::string expected_name =
        std::string(TESTS_DIR "/") + name + "/" + expected_filename;
    std::ifstream expected_file(expected_name);
    std::stringstream expected_stream;
    expected_stream << expected_file.rdbuf();
    auto expected = normalize_line_endings(expected_stream.str());

    arduboy->program_state.cfg.display_type = absim::display_t::type_t::SSD1306;
    arduboy->program_state.cfg.fxport_reg = 0x2b;
    arduboy->program_state.cfg.fxport_mask = 1 << 1;
    arduboy->program_state.cfg.bootloader = true;
    arduboy->program_state.cfg.boot_to_menu = false;
    arduboy->reset();
    arduboy->core_state.cpu.data[0x23] = 0x10;
    arduboy->core_state.cpu.data[0x2c] = 0x40;
    arduboy->core_state.cpu.data[0x2f] = 0xf0;

    auto const& d = arduboy->core_state.cpu.serial_bytes;
    for(int i = 0; i < 10000; ++i) // up to ten seconds
    {
        arduboy->advance(1'000'000'000ull); // 1 ms
        if(serial_has_end(d)) break;
        arduboy->core_state.cpu.sound_buffer.clear();
    }

    auto actual = normalize_line_endings(serial_bytes_to_string(d));
    bool pass = (actual == expected);

    printf("   %-30s : %s\n", name, pass ? "PASS" : "FAIL");
    if(!pass)
    {
        printf("      expected:\n%s", expected.c_str());
        printf("      actual:\n%s", actual.c_str());
    }
    return pass ? 0 : 1;
}

static void advance(int ms)
{
    advance(*arduboy, ms);
}

static int compare_image(char const* dir, int n)
{
    int r = 0;
    char ifname[256];
    snprintf(ifname, sizeof(ifname), "%s/%s/image%d.bin", TESTS_DIR, dir, n);
#if WRITE_IMAGES
    std::ofstream fi(ifname, std::ios::binary);
    fi.write((char const*)arduboy->peripherals.display.filtered_pixels.data(), 8192);
    snprintf(ifname, sizeof(ifname), "%s/%s/image%d.png", TESTS_DIR, dir, n);
    stbi_write_png(ifname, 128, 64, 1, arduboy->peripherals.display.filtered_pixels.data(), 128);
#else
    std::ifstream fi(ifname, std::ios::binary);
    std::vector<char> id;
    id.resize(8192);
    fi.read(id.data(), 8192);
    for(size_t i = 0; i < 8192; ++i)
    {
        bool w0 = (uint8_t)arduboy->peripherals.display.filtered_pixels[i] < 128;
        bool w1 = (uint8_t)id[i] < 128;
        if(w0 != w1)
            r = 1;
    }
#endif
    return r;
}

static int image_test(char const* dir, char const* game)
{
    std::ifstream f(std::string(TESTS_DIR "/") + dir + "/" + game, std::ios::binary);
    auto err = arduboy->load_file(game, f);
    int r = 0;
    if(!err.empty()) r = 1;
    arduboy->program_state.cfg.display_type = absim::display_t::type_t::SSD1306;
    arduboy->program_state.cfg.fxport_reg = 0x2b;
    arduboy->program_state.cfg.fxport_mask = 1 << 1;
    arduboy->program_state.cfg.bootloader = true;
    arduboy->program_state.cfg.boot_to_menu = false;
    arduboy->reset();

    int n = 0;

    arduboy->core_state.cpu.data[0x23] = 0x10;
    arduboy->core_state.cpu.data[0x2c] = 0x40;
    arduboy->core_state.cpu.data[0x2f] = 0xf0;
    advance(1000);
    r |= compare_image(dir, n++);
    for(int i = 0; i < 9; ++i)
    {
        advance(1000);
        arduboy->core_state.cpu.data[0x2c] = 0x00;
        if(i != 0)
            arduboy->core_state.cpu.data[0x2f] = 0xa0;
        advance(100);
        arduboy->core_state.cpu.data[0x2c] = 0x40;
        arduboy->core_state.cpu.data[0x2f] = 0xf0;
        advance(1000);
        r |= compare_image(dir, n++);
    }

    printf("   %-30s : %s\n", dir, r ? "FAIL" : "PASS");

    return r;
}

static std::string load_rom(absim::arduboy_t& a, char const* dir, char const* game)
{
    std::ifstream f(std::string(TESTS_DIR "/") + dir + "/" + game, std::ios::binary);
    return a.load_file(game, f);
}

static int hex_malformed_input_test()
{
    struct test_case_t
    {
        char const* name;
        char const* hex;
        char const* error;
    };

    test_case_t const cases[] =
    {
        { "bad byte count", ":GG000001FF", "HEX bad byte count" },
        { "bad address high byte", ":00GG0001FF", "HEX: bad address" },
        { "bad address low byte", ":0000GG01FF", "HEX: bad address" },
        { "bad type", ":000000GGFF", "HEX: bad type" },
        { "bad data", ":01000000G0", "HEX: bad data" },
        { "bad ignored-record data", ":040000030102GG04", "HEX: bad data" },
        { "bad checksum", ":00000001FE", "HEX: bad checksum" },
        {
            "non-zero EOF byte count",
            ":0100000100FE",
            "HEX: non-zero byte count at end-of-file record"
        },
        { "unsupported type", ":00000002FE", "HEX: unsupported type" },
    };

    int r = 0;
    for(auto const& c : cases)
    {
        auto a = std::make_unique<absim::arduboy_t>();
        std::stringstream f(c.hex);
        auto err = a->load_file("malformed.hex", f);
        bool pass = (err == c.error);
        if(!pass)
            r = 1;
        printf("   %-30s : %s\n", c.name, pass ? "PASS" : "FAIL");
    }
    return r;
}

static std::string save_savestate_bytes(absim::arduboy_t& a)
{
    std::ostringstream ss;
    auto err = a.save_savestate(ss);
    return err.empty() ? ss.str() : std::string();
}

static size_t libretro_serialize_size(absim::arduboy_t& a)
{
    return a.max_savestate_size();
}

static std::vector<uint8_t> save_savestate_libretro_bytes(absim::arduboy_t& a)
{
    size_t size = libretro_serialize_size(a);
    if(size == 0)
        return {};

    std::vector<uint8_t> data(size, 0xCD);
    absim::ostrstream s((char*)data.data(), (std::streamsize)data.size());
    auto err = a.save_savestate(s);
    if(!err.empty())
        return {};
    return data;
}

static bool same_stack_frame(
    absim::atmega32u4_t::stack_frame_t const& a,
    absim::atmega32u4_t::stack_frame_t const& b)
{
    return a.cycle == b.cycle && a.pc == b.pc && a.sp == b.sp;
}

static bool same_usb_endpoint(
    absim::atmega32u4_t::usb_endpoint_t const& a,
    absim::atmega32u4_t::usb_endpoint_t const& b)
{
    return a.uenum == b.uenum &&
        a.ueintx == b.ueintx &&
        a.uerst == b.uerst &&
        a.ueconx == b.ueconx &&
        a.uecfg0x == b.uecfg0x &&
        a.uecfg1x == b.uecfg1x &&
        a.uesta0x == b.uesta0x &&
        a.uesta1x == b.uesta1x &&
        a.ueienx == b.ueienx &&
        a.uebclx == b.uebclx &&
        a.uebchx == b.uebchx &&
        a.buffer == b.buffer &&
        a.buffer_size == b.buffer_size &&
        a.start == b.start &&
        a.length == b.length;
}

static bool same_timer8(absim::atmega32u4_t::timer8_t const& a, absim::atmega32u4_t::timer8_t const& b)
{
    return a.prev_update_cycle == b.prev_update_cycle &&
        a.next_update_cycle == b.next_update_cycle &&
        a.prescaler_cycle == b.prescaler_cycle &&
        a.divider == b.divider &&
        a.top == b.top &&
        a.tov == b.tov &&
        a.tcnt == b.tcnt &&
        a.ocrNa == b.ocrNa &&
        a.ocrNb == b.ocrNb &&
        a.phase_correct == b.phase_correct &&
        a.count_down == b.count_down &&
        a.update_ocrN_at_top == b.update_ocrN_at_top;
}

static bool same_timer16(absim::atmega32u4_t::timer16_t const& a, absim::atmega32u4_t::timer16_t const& b)
{
    return a.prev_update_cycle == b.prev_update_cycle &&
        a.next_update_cycle == b.next_update_cycle &&
        a.prescaler_cycle == b.prescaler_cycle &&
        a.divider == b.divider &&
        a.top == b.top &&
        a.tov == b.tov &&
        a.tcnt == b.tcnt &&
        a.ocrNa == b.ocrNa &&
        a.ocrNb == b.ocrNb &&
        a.ocrNc == b.ocrNc &&
        a.tifrN_addr == b.tifrN_addr &&
        a.timskN_addr == b.timskN_addr &&
        a.prr_addr == b.prr_addr &&
        a.prr_mask == b.prr_mask &&
        a.base_addr == b.base_addr &&
        a.temp == b.temp &&
        a.phase_correct == b.phase_correct &&
        a.count_down == b.count_down &&
        a.update_ocrN_at_top == b.update_ocrN_at_top &&
        a.update_ocrN_at_bottom == b.update_ocrN_at_bottom &&
        a.fast_pwm == b.fast_pwm &&
        a.top_source_icr == b.top_source_icr;
}

static bool same_timer10(absim::atmega32u4_t::timer10_t const& a, absim::atmega32u4_t::timer10_t const& b)
{
    return a.prev_update_cycle == b.prev_update_cycle &&
        a.next_update_cycle == b.next_update_cycle &&
        a.divider_cycle == b.divider_cycle &&
        a.divider == b.divider &&
        a.top == b.top &&
        a.tov == b.tov &&
        a.tcnt == b.tcnt &&
        a.ocrNa == b.ocrNa &&
        a.ocrNa_next == b.ocrNa_next &&
        a.ocrNb == b.ocrNb &&
        a.ocrNb_next == b.ocrNb_next &&
        a.ocrNc == b.ocrNc &&
        a.ocrNc_next == b.ocrNc_next &&
        a.ocrNd == b.ocrNd &&
        a.ocrNd_next == b.ocrNd_next &&
        a.async_cycle == b.async_cycle &&
        a.com4a == b.com4a &&
        a.tlock == b.tlock &&
        a.enhc == b.enhc &&
        a.phase_correct == b.phase_correct &&
        a.count_down == b.count_down &&
        a.update_ocrN_at_top == b.update_ocrN_at_top &&
        a.update_ocrN_at_bottom == b.update_ocrN_at_bottom;
}

static bool same_cpu_state(absim::atmega32u4_t const& a, absim::atmega32u4_t const& b)
{
    bool same = a.data == b.data &&
        a.active == b.active &&
        a.wakeup_cycles == b.wakeup_cycles &&
        a.just_interrupted == b.just_interrupted &&
        a.min_stack == b.min_stack &&
        a.stack_check == b.stack_check &&
        a.pushed_at_least_once == b.pushed_at_least_once &&
        a.eeprom == b.eeprom &&
        a.eeprom_modified_bytes == b.eeprom_modified_bytes &&
        a.eeprom_modified == b.eeprom_modified &&
        a.eeprom_dirty == b.eeprom_dirty &&
        a.prev_sreg == b.prev_sreg &&
        a.pc == b.pc &&
        a.executing_instr_pc == b.executing_instr_pc &&
        a.num_stack_frames == b.num_stack_frames &&
        a.program_loaded == b.program_loaded &&
        a.lock == b.lock &&
        a.fuse_lo == b.fuse_lo &&
        a.fuse_hi == b.fuse_hi &&
        a.fuse_ext == b.fuse_ext &&
        same_timer8(a.timer0, b.timer0) &&
        same_timer16(a.timer1, b.timer1) &&
        same_timer16(a.timer3, b.timer3) &&
        same_timer10(a.timer4, b.timer4) &&
        a.pll_prev_cycle == b.pll_prev_cycle &&
        a.pll_lock_cycle == b.pll_lock_cycle &&
        a.pll_num12 == b.pll_num12 &&
        a.pll_busy == b.pll_busy &&
        a.spsr_read_after_transmit == b.spsr_read_after_transmit &&
        a.spi_busy == b.spi_busy &&
        a.spi_busy_clear == b.spi_busy_clear &&
        a.spi_latch_read == b.spi_latch_read &&
        a.spi_data_latched == b.spi_data_latched &&
        a.spi_data_byte == b.spi_data_byte &&
        a.spi_datain_byte == b.spi_datain_byte &&
        a.spi_done_cycle == b.spi_done_cycle &&
        a.spi_transmit_zero_cycle == b.spi_transmit_zero_cycle &&
        a.spi_clock_cycles == b.spi_clock_cycles &&
        a.eeprom_prev_cycle == b.eeprom_prev_cycle &&
        a.eeprom_clear_eempe_cycles == b.eeprom_clear_eempe_cycles &&
        a.eeprom_write_addr == b.eeprom_write_addr &&
        a.eeprom_write_data == b.eeprom_write_data &&
        a.eeprom_program_cycles == b.eeprom_program_cycles &&
        a.eeprom_busy == b.eeprom_busy &&
        a.adc_prev_cycle == b.adc_prev_cycle &&
        a.adc_prescaler_cycle == b.adc_prescaler_cycle &&
        a.adc_cycle == b.adc_cycle &&
        a.adc_ref == b.adc_ref &&
        a.adc_result == b.adc_result &&
        a.adc_seed == b.adc_seed &&
        a.adc_busy == b.adc_busy &&
        a.adc_nondeterminism == b.adc_nondeterminism &&
        a.sound_prev_cycle == b.sound_prev_cycle &&
        a.sound_cycle == b.sound_cycle &&
        a.sound_enabled == b.sound_enabled &&
        a.sound_pwm == b.sound_pwm &&
        a.sound_pwm_val == b.sound_pwm_val &&
        a.usb_next_sofi_cycle == b.usb_next_sofi_cycle &&
        a.usb_next_eorsti_cycle == b.usb_next_eorsti_cycle &&
        a.usb_next_setconf_cycle == b.usb_next_setconf_cycle &&
        a.usb_next_setlength_cycle == b.usb_next_setlength_cycle &&
        a.usb_next_update_cycle == b.usb_next_update_cycle &&
        a.usb_dpram == b.usb_dpram &&
        a.usb_attached == b.usb_attached &&
        a.spm_prev_cycle == b.spm_prev_cycle &&
        a.spm_busy == b.spm_busy &&
        a.spm_en_cycles == b.spm_en_cycles &&
        a.spm_op == b.spm_op &&
        a.spm_cycles == b.spm_cycles &&
        a.spm_buffer == b.spm_buffer &&
        a.watchdog_divider == b.watchdog_divider &&
        a.watchdog_divider_cycle == b.watchdog_divider_cycle &&
        a.watchdog_prev_cycle == b.watchdog_prev_cycle &&
        a.watchdog_next_cycle == b.watchdog_next_cycle &&
        a.peripheral_queue.next_cycle() == b.peripheral_queue.next_cycle() &&
        a.cycle_count == b.cycle_count;

    if(!same) return false;

    for(size_t i = 0; i < a.stack_frames.size(); ++i)
        if(!same_stack_frame(a.stack_frames[i], b.stack_frames[i]))
            return false;

    for(size_t i = 0; i < a.usb_ep.size(); ++i)
        if(!same_usb_endpoint(a.usb_ep[i], b.usb_ep[i]))
            return false;

    return true;
}

static bool same_display_state(absim::display_t const& a, absim::display_t const& b)
{
    return a.filtered_pixels == b.filtered_pixels &&
        a.filtered_pixel_counts == b.filtered_pixel_counts &&
        a.type == b.type &&
        a.pixels == b.pixels &&
        a.pixel_history_index == b.pixel_history_index &&
        a.enable_filter == b.enable_filter &&
        a.ram == b.ram &&
        a.ref_segment_current == b.ref_segment_current &&
        a.current_limit_slope == b.current_limit_slope &&
        a.enable_current_limiting == b.enable_current_limiting &&
        a.prev_row_drive == b.prev_row_drive &&
        a.contrast == b.contrast &&
        a.entire_display_on == b.entire_display_on &&
        a.inverse_display == b.inverse_display &&
        a.display_on == b.display_on &&
        a.enable_charge_pump == b.enable_charge_pump &&
        a.addressing_mode == b.addressing_mode &&
        a.col_start == b.col_start &&
        a.col_end == b.col_end &&
        a.page_start == b.page_start &&
        a.page_end == b.page_end &&
        a.mux_ratio == b.mux_ratio &&
        a.display_offset == b.display_offset &&
        a.display_start == b.display_start &&
        a.com_scan_direction == b.com_scan_direction &&
        a.alternative_com == b.alternative_com &&
        a.com_remap == b.com_remap &&
        a.segment_remap == b.segment_remap &&
        a.fosc_index == b.fosc_index &&
        a.divide_ratio == b.divide_ratio &&
        a.phase_1 == b.phase_1 &&
        a.phase_2 == b.phase_2 &&
        a.vcomh_deselect == b.vcomh_deselect &&
        a.row == b.row &&
        a.row_cycle == b.row_cycle &&
        a.cycles_per_row == b.cycles_per_row &&
        a.ps_per_clk == b.ps_per_clk &&
        a.ps_rem == b.ps_rem &&
        a.processing_command == b.processing_command &&
        a.current_command == b.current_command &&
        a.command_byte_index == b.command_byte_index &&
        a.data_page == b.data_page &&
        a.data_col == b.data_col &&
        a.vsync == b.vsync;
}

static bool same_fx_sector(
    absim::w25q128_t::sector_t const& a,
    absim::w25q128_t::sector_t const& b)
{
    return a == b;
}

static bool same_fx_state(absim::w25q128_t const& a, absim::w25q128_t const& b)
{
    if(a.sectors_modified_data.size() != b.sectors_modified_data.size())
        return false;
    for(size_t i = 0; i < a.sectors_modified_data.size(); ++i)
    {
        bool ahas = (bool)a.sectors_modified_data[i];
        bool bhas = (bool)b.sectors_modified_data[i];
        if(ahas != bhas) return false;
        if(ahas && !same_fx_sector(*a.sectors_modified_data[i], *b.sectors_modified_data[i]))
            return false;
    }

    return a.sectors_modified == b.sectors_modified &&
        a.sectors_dirty == b.sectors_dirty &&
        a.enabled == b.enabled &&
        a.woken_up == b.woken_up &&
        a.write_enabled == b.write_enabled &&
        a.reading_status == b.reading_status &&
        a.processing_command == b.processing_command &&
        a.reading == b.reading &&
        a.programming == b.programming &&
        a.erasing_sector == b.erasing_sector &&
        a.releasing == b.releasing &&
        a.reading_jedec_id == b.reading_jedec_id &&
        a.busy_ps_rem == b.busy_ps_rem &&
        a.current_addr == b.current_addr &&
        a.command == b.command &&
        a.min_page == b.min_page &&
        a.max_page == b.max_page &&
        a.busy_error == b.busy_error;
}

static bool same_runtime_state(absim::arduboy_t const& a, absim::arduboy_t const& b)
{
    return same_cpu_state(a.core_state.cpu, b.core_state.cpu) &&
        same_display_state(a.peripherals.display, b.peripherals.display) &&
        same_fx_state(a.peripherals.fx, b.peripherals.fx) &&
        a.program_state.prog_filename == b.program_state.prog_filename &&
        a.program_state.prog_filedata == b.program_state.prog_filedata &&
        a.peripherals.fxport_reg == b.peripherals.fxport_reg &&
        a.peripherals.fxport_mask == b.peripherals.fxport_mask &&
        a.program_state.game_hash == b.program_state.game_hash &&
        a.program_state.title == b.program_state.title &&
        a.program_state.device_type == b.program_state.device_type &&
        a.peripherals.prev_display_reset == b.peripherals.prev_display_reset;
}

static bool same_snapshot_state(absim::arduboy_t const& a, absim::arduboy_t const& b)
{
    if(a.core_state.cpu.serial_bytes != b.core_state.cpu.serial_bytes ||
        a.core_state.cpu.sound_buffer != b.core_state.cpu.sound_buffer ||
        a.peripherals.fx.sectors.size() != b.peripherals.fx.sectors.size() ||
        a.profiler_state.total != b.profiler_state.total ||
        a.profiler_state.total_with_sleep != b.profiler_state.total_with_sleep ||
        a.profiler_state.prev_frame_cycles != b.profiler_state.prev_frame_cycles ||
        a.profiler_state.total_frames != b.profiler_state.total_frames ||
        a.profiler_state.prev_ms_cycles != b.profiler_state.prev_ms_cycles ||
        a.profiler_state.total_ms != b.profiler_state.total_ms ||
        a.profiler_state.frame_bytes_total != b.profiler_state.frame_bytes_total ||
        a.profiler_state.frame_bytes != b.profiler_state.frame_bytes ||
        a.profiler_state.frame_cpu_usage != b.profiler_state.frame_cpu_usage ||
        a.profiler_state.ms_cpu_usage_raw != b.profiler_state.ms_cpu_usage_raw ||
        a.profiler_state.ms_cpu_usage != b.profiler_state.ms_cpu_usage ||
        a.profiler_state.enabled != b.profiler_state.enabled ||
        a.profiler_state.cached_total != b.profiler_state.cached_total ||
        a.profiler_state.cached_total_with_sleep != b.profiler_state.cached_total_with_sleep ||
        a.profiler_state.num_hotspots != b.profiler_state.num_hotspots ||
        a.debugger_state.breakpoints != b.debugger_state.breakpoints ||
        a.debugger_state.breakpoints_rd != b.debugger_state.breakpoints_rd ||
        a.debugger_state.breakpoints_wr != b.debugger_state.breakpoints_wr ||
        a.debugger_state.paused != b.debugger_state.paused ||
        a.program_state.cfg.display_type != b.program_state.cfg.display_type ||
        a.program_state.cfg.fxport_reg != b.program_state.cfg.fxport_reg ||
        a.program_state.cfg.fxport_mask != b.program_state.cfg.fxport_mask ||
        a.program_state.cfg.bootloader != b.program_state.cfg.bootloader ||
        a.program_state.cfg.boot_to_menu != b.program_state.cfg.boot_to_menu ||
        a.program_state.flashcart_loaded != b.program_state.flashcart_loaded ||
        a.debugger_state.input_history.size() != b.debugger_state.input_history.size() ||
        a.debugger_state.state_history.size() != b.debugger_state.state_history.size() ||
        a.debugger_state.present_state != b.debugger_state.present_state ||
        a.debugger_state.present_cycle != b.debugger_state.present_cycle)
    {
        return false;
    }

    if(a.profiler_state.hotspots_symbol.size() != b.profiler_state.hotspots_symbol.size())
        return false;

    for(size_t i = 0; i < a.profiler_state.hotspots_symbol.size(); ++i)
    {
        auto const& x = a.profiler_state.hotspots_symbol[i];
        auto const& y = b.profiler_state.hotspots_symbol[i];
        if(x.count != y.count || x.begin != y.begin || x.end != y.end)
            return false;
    }

    for(size_t i = 0; i < a.profiler_state.hotspots.size(); ++i)
    {
        auto const& x = a.profiler_state.hotspots[i];
        auto const& y = b.profiler_state.hotspots[i];
        if(x.count != y.count || x.begin != y.begin || x.end != y.end)
            return false;
    }

    for(size_t i = 0; i < a.debugger_state.input_history.size(); ++i)
    {
        auto const& x = a.debugger_state.input_history[i];
        auto const& y = b.debugger_state.input_history[i];
        if(x.cycle != y.cycle || x.pinb != y.pinb || x.pine != y.pine || x.pinf != y.pinf)
            return false;
    }

    for(size_t i = 0; i < a.debugger_state.state_history.size(); ++i)
    {
        auto const& x = a.debugger_state.state_history[i];
        auto const& y = b.debugger_state.state_history[i];
        if(x.cycle != y.cycle || x.state != y.state)
            return false;
    }

    for(size_t i = 0; i < a.peripherals.fx.sectors.size(); ++i)
    {
        bool ahas = (bool)a.peripherals.fx.sectors[i];
        bool bhas = (bool)b.peripherals.fx.sectors[i];
        if(ahas != bhas) return false;
        if(ahas && !same_fx_sector(*a.peripherals.fx.sectors[i], *b.peripherals.fx.sectors[i]))
            return false;
    }

    return true;
}

static int grouped_state_roundtrip_test()
{
    auto original = std::make_unique<absim::arduboy_t>();
    auto restored = std::make_unique<absim::arduboy_t>();

    auto err = load_rom(*original, snapshot_name, snapshot_rom);
    if(!err.empty())
    {
        printf("   %-30s : FAIL\n", "grouped state roundtrip");
        return 1;
    }
    err = load_rom(*restored, snapshot_name, snapshot_rom);
    if(!err.empty())
    {
        printf("   %-30s : FAIL\n", "grouped state roundtrip");
        return 1;
    }

    original->reset();
    restored->reset();
    original->program_state.cfg.display_type = absim::display_t::type_t::SSD1306;
    original->program_state.cfg.fxport_reg = 0x2b;
    original->program_state.cfg.fxport_mask = 1 << 1;
    original->program_state.cfg.bootloader = true;
    original->program_state.cfg.boot_to_menu = false;
    original->peripherals.display.enable_filter = true;
    original->core_state.cpu.data[0x23] = 0x10;
    original->core_state.cpu.data[0x2c] = 0x40;
    original->core_state.cpu.data[0x2f] = 0xf0;
    advance(*original, 250);

    std::stringstream state;
    err = original->save_savestate(state);
    if(!err.empty())
    {
        printf("   %-30s : FAIL\n", "grouped state roundtrip");
        return 1;
    }

    err = restored->load_savestate(state);
    bool r = !err.empty() || !same_runtime_state(*original, *restored);

    advance(*original, 250);
    advance(*restored, 250);
    r = r || !same_runtime_state(*original, *restored);
    r = r || save_savestate_bytes(*original) != save_savestate_bytes(*restored);

    printf("   %-30s : %s\n", "grouped state roundtrip", r ? "FAIL" : "PASS");
    return r ? 1 : 0;
}

static int savestate_snapshot_test()
{
    auto original = std::make_unique<absim::arduboy_t>();
    auto restored = std::make_unique<absim::arduboy_t>();

    auto err = load_rom(*original, snapshot_name, snapshot_rom);
    if(!err.empty())
    {
        printf("   %-30s : FAIL\n", "savestate roundtrip");
        return 1;
    }
    err = load_rom(*restored, snapshot_name, snapshot_rom);
    if(!err.empty())
    {
        printf("   %-30s : FAIL\n", "savestate roundtrip");
        return 1;
    }

    original->reset();
    restored->reset();
    advance(*original, 250);

    std::stringstream state;
    err = original->save_savestate(state);
    if(!err.empty())
    {
        printf("   %-30s : FAIL\n", "savestate roundtrip");
        return 1;
    }

    err = restored->load_savestate(state);
    bool r = !err.empty() || !same_runtime_state(*original, *restored);

    advance(*original, 250);
    advance(*restored, 250);
    r = r || !same_runtime_state(*original, *restored);
    r = r || save_savestate_bytes(*original) != save_savestate_bytes(*restored);

    printf("   %-30s : %s\n", "savestate roundtrip", r ? "FAIL" : "PASS");
    return r ? 1 : 0;
}

static int savestate_libretro_roundtrip_test()
{
    auto original = std::make_unique<absim::arduboy_t>();
    auto restored = std::make_unique<absim::arduboy_t>();

    auto err = load_rom(*original, snapshot_name, snapshot_rom);
    if(!err.empty())
    {
        printf("   %-30s : FAIL\n", "libretro savestate roundtrip");
        return 1;
    }
    err = load_rom(*restored, snapshot_name, snapshot_rom);
    if(!err.empty())
    {
        printf("   %-30s : FAIL\n", "libretro savestate roundtrip");
        return 1;
    }

    original->reset();
    restored->reset();
    advance(*original, 250);

    auto reference = save_savestate_bytes(*original);
    auto state = save_savestate_libretro_bytes(*original);
    if(state.empty())
    {
        printf("   %-30s : FAIL\n", "libretro savestate roundtrip");
        return 1;
    }

    absim::istrstream s((char const*)state.data(), (std::streamsize)state.size());
    err = restored->load_savestate(s);
    bool r = !err.empty() || !same_runtime_state(*original, *restored);

    advance(*original, 250);
    advance(*restored, 250);
    r = r || !same_runtime_state(*original, *restored);
    r = r || save_savestate_bytes(*original) != save_savestate_bytes(*restored);

    printf("   %-30s : %s\n", "libretro savestate roundtrip", r ? "FAIL" : "PASS");
    return r ? 1 : 0;
}

static int full_snapshot_test()
{
    auto original = std::make_unique<absim::arduboy_t>();
    auto restored = std::make_unique<absim::arduboy_t>();

    original->program_state.cfg.display_type = absim::display_t::type_t::SSD1306;
    original->program_state.cfg.fxport_reg = 0x2b;
    original->program_state.cfg.fxport_mask = 1 << 1;
    original->program_state.cfg.bootloader = true;
    original->program_state.cfg.boot_to_menu = false;

    auto err = load_rom(*original, snapshot_name, snapshot_rom);
    if(!err.empty())
    {
        printf("   %-30s : FAIL\n", "snapshot roundtrip");
        return 1;
    }

    original->reset();
    original->core_state.cpu.data[0x23] = 0x10;
    original->core_state.cpu.data[0x2c] = 0x40;
    original->core_state.cpu.data[0x2f] = 0xf0;
    advance(*original, 500);

    std::stringstream snapshot;
    if(!original->save_snapshot(snapshot))
    {
        printf("   %-30s : FAIL\n", "snapshot roundtrip");
        return 1;
    }
    snapshot.clear();
    snapshot.seekg(0);

    err = restored->load_snapshot(snapshot);
    bool r = !err.empty() ||
        !same_runtime_state(*original, *restored) ||
        !same_snapshot_state(*original, *restored);

    advance(*original, 250);
    advance(*restored, 250);
    r = r || !same_runtime_state(*original, *restored);
    r = r || !same_snapshot_state(*original, *restored);

    std::stringstream snapshot2;
    r = r || !restored->save_snapshot(snapshot2);

    printf("   %-30s : %s\n", "snapshot roundtrip", r ? "FAIL" : "PASS");
    return r ? 1 : 0;
}

int main(int argc, char** argv)
{
    int r = 0;
    arduboy = std::make_unique<absim::arduboy_t>();
    arduboy->peripherals.display.enable_filter = true;

    printf("\nGrouped state tests...\n");
    r |= grouped_state_roundtrip_test();

    printf("\nSnapshot tests...\n");
    r |= savestate_snapshot_test();
    r |= savestate_libretro_roundtrip_test();
    r |= full_snapshot_test();

    printf("\nIntegration tests...\n");
    r |= test("float");
    r |= test("instructions");
    r |= test("signature");
    r |= test("timer_tcnt_write");
    r |= serial_output_test("timer_cycle_accuracy", "expected_serial.txt");

    printf("\nHEX parser tests...\n");
    r |= hex_malformed_input_test();

    printf("\nImage tests...\n");
    r |= image_test("arduchess", "arduchess.hex");
    r |= image_test("ardugolf", "ardugolf.hex");
    r |= image_test("ardugolf_fx", "ardugolf_fx.arduboy");
    r |= image_test("dazzledash", "dazzledash.arduboy");
    r |= image_test("summercamp", "summercamp.arduboy");

    printf("\n");

    return r;
}
