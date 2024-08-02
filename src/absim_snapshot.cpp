#include "absim.hpp"

#include <sstream>
#include <tuple>

#include <bitsery/bitsery.h>
#include <bitsery/brief_syntax.h>
#include <bitsery/brief_syntax/array.h>
#include <bitsery/brief_syntax/map.h>
#include <bitsery/brief_syntax/string.h>
#include <bitsery/brief_syntax/vector.h>
#include <bitsery/ext/std_bitset.h>
#include <bitsery/ext/std_map.h>
#include <bitsery/adapter/buffer.h>
#include <bitsery/adapter/stream.h>
#include <bitsery/traits/vector.h>

#include <miniz.h>

static constexpr std::array<char, 8> SNAPSHOT_ID =
{
    '_', 'A', 'B', 'S', 'I', 'M', '_', '\0',
};

struct version_t
{
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    constexpr bool operator!=(version_t const& v) const
    {
        return major != v.major || minor != v.minor || patch != v.patch;
    }
    template<class A> void serialize(A& a)
    {
        a(major, minor, patch);
    }
};

static std::string version_str(version_t const& v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "v%u.%u.%u", v.major, v.minor, v.patch);
    return buf;
}

constexpr version_t VERSION_INFO =
{
    ARDENS_VERSION_MAJOR,
    ARDENS_VERSION_MINOR,
    ARDENS_VERSION_PATCH
};

namespace bitsery
{
template<typename S, size_t N>
void serialize(S& s, std::bitset<N>& obj)
{
    s.ext(obj, bitsery::ext::StdBitset{});
}
}

namespace absim
{

template<typename CharT, typename TraitsT = std::char_traits<CharT> >
class vectorwrapbuf : public std::basic_streambuf<char, std::char_traits<char>>
{
    std::vector<CharT>& vec;
public:
    vectorwrapbuf(std::vector<CharT>& v)
        : vec(v)
    {
        setg((char*)vec.data(), (char*)vec.data(), (char*)vec.data() + vec.size());
    }
    virtual pos_type seekpos(
        pos_type pos,
        std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override
    {
        setg((char*)vec.data(), (char*)vec.data() + pos, (char*)vec.data() + vec.size());
        return pos;
    }
};

template<class Archive>
static std::string serdes_savestate(Archive& ar, arduboy_t& a)
{
    ar(a.cpu.data);
    ar(a.cpu.just_read);
    ar(a.cpu.just_written);
    ar(a.cpu.active);
    ar(a.cpu.wakeup_cycles);
    ar(a.cpu.just_interrupted);
    ar(a.cpu.min_stack);
    ar(a.cpu.stack_check);
    ar(a.cpu.pushed_at_least_once);
    ar(a.cpu.eeprom);
    ar(a.cpu.eeprom_modified_bytes);
    ar(a.cpu.eeprom_modified);
    ar(a.cpu.eeprom_dirty);
    ar(a.cpu.prev_sreg);
    ar(a.cpu.pc);
    ar(a.cpu.executing_instr_pc);
    for(auto& f : a.cpu.stack_frames)
    {
        ar(f.pc);
        ar(f.sp);
    }
    ar(a.cpu.num_stack_frames);
    ar(a.cpu.program_loaded);
    ar(a.cpu.timer0);
    ar(a.cpu.timer1);
    ar(a.cpu.timer3);
    ar(a.cpu.timer4);

    ar(a.cpu.pll_prev_cycle);
    ar(a.cpu.pll_lock_cycle);
    ar(a.cpu.pll_num12);
    ar(a.cpu.pll_busy);

    ar(a.cpu.spsr_read_after_transmit);
    ar(a.cpu.spi_busy);
    ar(a.cpu.spi_busy_clear);
    ar(a.cpu.spi_latch_read);
    ar(a.cpu.spi_data_latched);
    ar(a.cpu.spi_data_byte);
    ar(a.cpu.spi_datain_byte);
    ar(a.cpu.spi_done_cycle);
    ar(a.cpu.spi_transmit_zero_cycle);
    ar(a.cpu.spi_clock_cycles);

    ar(a.cpu.eeprom_prev_cycle);
    ar(a.cpu.eeprom_clear_eempe_cycles);
    ar(a.cpu.eeprom_write_addr);
    ar(a.cpu.eeprom_write_data);
    ar(a.cpu.eeprom_program_cycles);
    ar(a.cpu.eeprom_busy);

    ar(a.cpu.adc_prev_cycle);
    ar(a.cpu.adc_prescaler_cycle);
    ar(a.cpu.adc_cycle);
    ar(a.cpu.adc_ref);
    ar(a.cpu.adc_result);
    ar(a.cpu.adc_seed);
    ar(a.cpu.adc_busy);
    ar(a.cpu.adc_nondeterminism);

    ar(a.cpu.sound_cycle);
    ar(a.cpu.sound_enabled);
    ar(a.cpu.sound_pwm);
    ar(a.cpu.sound_pwm_val);
    ar(a.cpu.sound_buffer);

    ar(a.cpu.serial_bytes);
    ar(a.cpu.usb_ep);
    ar(a.cpu.usb_next_sofi_cycle);
    ar(a.cpu.usb_next_eorsti_cycle);
    ar(a.cpu.usb_next_setconf_cycle);
    ar(a.cpu.usb_next_setlength_cycle);
    ar(a.cpu.usb_next_update_cycle);
    ar(a.cpu.usb_dpram);
    ar(a.cpu.usb_attached);

    ar(a.cpu.spm_prev_cycle);
    ar(a.cpu.spm_busy);
    ar(a.cpu.spm_en_cycles);
    ar(a.cpu.spm_op);
    ar(a.cpu.spm_cycles);
    ar(a.cpu.spm_buffer);

    ar(a.cpu.watchdog_divider);
    ar(a.cpu.watchdog_divider_cycle);

    ar(a.cpu.cycle_count);

    ar(a.cpu.lock);
    ar(a.cpu.fuse_lo);
    ar(a.cpu.fuse_hi);
    ar(a.cpu.fuse_ext);

    ar(a.display.filtered_pixels);
    ar(a.display.filtered_pixel_counts);
    ar(a.display.type);
    ar(a.display.pixels);
    ar(a.display.pixel_history_index);
    ar(a.display.enable_filter);
    ar(a.display.ram);
    ar(a.display.ref_segment_current);
    ar(a.display.current_limit_slope);
    ar(a.display.enable_current_limiting);
    ar(a.display.prev_row_drive);
    ar(a.display.contrast);
    ar(a.display.entire_display_on);
    ar(a.display.inverse_display);
    ar(a.display.display_on);
    ar(a.display.enable_charge_pump);
    ar(a.display.addressing_mode);
    ar(a.display.col_start);
    ar(a.display.col_end);
    ar(a.display.page_start);
    ar(a.display.page_end);
    ar(a.display.mux_ratio);
    ar(a.display.display_offset);
    ar(a.display.display_start);
    ar(a.display.com_scan_direction);
    ar(a.display.alternative_com);
    ar(a.display.com_remap);
    ar(a.display.segment_remap);
    ar(a.display.fosc_index);
    ar(a.display.divide_ratio);
    ar(a.display.phase_1);
    ar(a.display.phase_2);
    ar(a.display.vcomh_deselect);
    ar(a.display.row);
    ar(a.display.row_cycle);
    ar(a.display.cycles_per_row);
    ar(a.display.ps_per_clk);
    ar(a.display.ps_rem);
    ar(a.display.processing_command);
    ar(a.display.current_command);
    ar(a.display.command_byte_index);
    ar(a.display.data_page);
    ar(a.display.data_col);
    ar(a.display.vsync);

    ar(a.fx.data);
    ar(a.fx.sectors_modified);
    ar(a.fx.sectors_dirty);
    ar(a.fx.enabled);
    ar(a.fx.woken_up);
    ar(a.fx.write_enabled);
    ar(a.fx.reading_status);
    ar(a.fx.processing_command);
    ar(a.fx.reading);
    ar(a.fx.programming);
    ar(a.fx.erasing_sector);
    ar(a.fx.releasing);
    ar(a.fx.busy_ps_rem);
    ar(a.fx.current_addr);
    ar(a.fx.command);
    ar(a.fx.min_page);
    ar(a.fx.max_page);
    ar(a.fx.busy_error);

    ar(a.fxport_reg);
    ar(a.fxport_mask);
    ar(a.game_hash);
    ar(a.title);
    ar(a.device_type);
    ar(a.prev_display_reset);

    return "";
}

template<bool is_load, class Archive>
static std::string serdes_snapshot(Archive& ar, arduboy_t& a)
{
    ar(a.prog_filename);
    ar(a.prog_filedata);

    if(is_load)
    {
        vectorwrapbuf<uint8_t> vb(a.prog_filedata);
        std::istream is(&vb);
        auto r = a.load_file(a.prog_filename.c_str(), is);
        if(!r.empty())
            return r;
    }

    ar(a.profiler_hotspots_symbol);
    ar(a.profiler_total);
    ar(a.profiler_total_with_sleep);
    ar(a.prev_frame_cycles);
    ar(a.total_frames);
    ar(a.prev_ms_cycles);
    ar(a.total_ms);
    ar(a.frame_bytes_total);
    ar(a.frame_bytes);
    ar(a.frame_cpu_usage);
    ar(a.ms_cpu_usage);
    ar(a.profiler_enabled);
    ar(a.cached_profiler_total);
    ar(a.cached_profiler_total_with_sleep);
    ar(a.profiler_hotspots);
    ar(a.num_hotspots);

    ar(a.breakpoints);
    ar(a.breakpoints_rd);
    ar(a.breakpoints_wr);
    ar(a.paused);

    ar(a.cfg.display_type);
    ar(a.cfg.fxport_reg);
    ar(a.cfg.fxport_mask);
    ar(a.cfg.bootloader);
    ar(a.cfg.boot_to_menu);
    ar(a.flashcart_loaded);

    return serdes_savestate(ar, a);
}

bool arduboy_t::save_savestate(std::ostream& f)
{
    bitsery::Serializer<bitsery::OutputStreamAdapter> ar(f);
    auto r = serdes_savestate(ar, *this);
    return !r.empty();
}

std::string arduboy_t::load_savestate(std::istream& f)
{
    bitsery::Deserializer<bitsery::InputStreamAdapter> ar(f);
    auto r = serdes_savestate(ar, *this);
    return r;
}

bool arduboy_t::save_snapshot(std::ostream& f)
{
    using Buffer = std::vector<uint8_t>;
    using BufferAdapter = bitsery::OutputBufferAdapter<Buffer>;
    using StreamAdapter = bitsery::OutputStreamAdapter;

    std::ostringstream ss;
    Buffer data;

    // serialize
    {
        bitsery::Serializer<BufferAdapter> ar(data);
        auto r = serdes_snapshot<false>(ar, *this);
        if(!r.empty()) return false;
    }

    // compress
    std::vector<uint8_t> dst;
    dst.resize(mz_compressBound((mz_ulong)data.size()));
    mz_ulong dst_size = (mz_ulong)dst.size();
    if(MZ_OK != mz_compress2(
        dst.data(), &dst_size,
        (uint8_t const*)data.data(), (mz_ulong)data.size(),
        MZ_BEST_COMPRESSION))
        return false;

    {
        bitsery::Serializer<StreamAdapter> ar(f);
        ar(SNAPSHOT_ID);
        ar(VERSION_INFO);
        uint32_t tsize = (uint32_t)data.size();
        ar(tsize);
    }
    f.write((char const*)dst.data(), dst_size);
    return true;
}

std::string arduboy_t::load_snapshot(std::istream& f)
{
    using Buffer = std::vector<char>;
    using BufferAdapter = bitsery::InputBufferAdapter<Buffer>;
    using StreamAdapter = bitsery::InputStreamAdapter;

    // uncompress

    std::istringstream ss;
    uint32_t dst_size = 0;

    {
        bitsery::Deserializer<StreamAdapter> ar(f);

        std::array<char, 8> id;
        ar(id);
        if(id != SNAPSHOT_ID)
            return "Snapshot: invalid identifier";

        version_t version;
        ar(version);
        if(version != VERSION_INFO)
            return "Snapshot: requires " + version_str(version);

        ar(dst_size);

        if(dst_size >= 256 * 1024 * 1024)
            return "Snapshot: data too large";
    }

    std::vector<char> data(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());

    std::vector<char> dst;
    dst.resize(dst_size);

    mz_ulong dst_size2 = (mz_ulong)dst_size;
    mz_ulong src_size2 = (mz_ulong)data.size();

    if(MZ_OK != mz_uncompress2(
        (uint8_t*)dst.data(), &dst_size2,
        (uint8_t const*)data.data(), &src_size2))
        return "Unable to uncompress snapshot";

    // deserialize
    {
        bitsery::Deserializer<BufferAdapter> ar(dst.begin(), dst.end());
        auto r = serdes_snapshot<true>(ar, *this);
        if(!r.empty()) return r;
    }

    return "";
}

void arduboy_t::save_savedata(std::ostream& f)
{
    using StreamAdapter = bitsery::OutputStreamAdapter;
    bitsery::Serializer<StreamAdapter> ar(f);
    savedata.game_hash = game_hash;
    ar(savedata);
}

bool arduboy_t::load_savedata(std::istream& f)
{
    using StreamAdapter = bitsery::InputStreamAdapter;
    bitsery::Deserializer<StreamAdapter> ar(f);
    savedata.clear();
    ar(savedata);
    if(savedata.game_hash != game_hash)
    {
        savedata.clear();
        return false;
    }

    // overwrite eeprom / fx with saved data

    auto const& d = savedata;
    if(d.eeprom.size() == cpu.eeprom.size())
        memcpy(cpu.eeprom.data(), d.eeprom.data(), array_bytes(cpu.eeprom));

    for(auto const& kv : d.fx_sectors)
    {
        uint32_t sector = kv.first;
        auto const& sdata = kv.second;
        if(sector >= fx.NUM_SECTORS) continue;
        memcpy(&fx.data[sector * 4096], sdata.data(), 4096);
    }

    return true;
}

}
