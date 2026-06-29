#include "absim.hpp"

#include <sstream>
#include <tuple>

#include <cstdio>

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

#include "absim_strstream.hpp"

constexpr std::array<char, 8> SNAPSHOT_ID =
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

constexpr version_t VERSION_INFO =
{
    ARDENS_VERSION_MAJOR,
    ARDENS_VERSION_MINOR,
    ARDENS_VERSION_PATCH
};

constexpr version_t SNAPSHOT_VERSION = { 0, 24, 18 };

static std::string version_str(version_t const& v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "v%u.%u.%u", v.major, v.minor, v.patch);
    return buf;
}

struct CustomUniquePtrExt
{
    template<class Ser, class T, class F>
    void serialize(Ser& ser, T const& obj, F&& f) const
    {
        ser.boolValue(static_cast<bool>(obj));
        if(obj) f(ser, *obj);
    }
    template<class Des, class T, class F>
    void deserialize(Des& des, T& obj, F&& f) const
    {
        bool exists = false;
        des.boolValue(exists);
        if(exists)
        {
            obj = std::make_unique<typename T::element_type>();
            f(des, *obj);
        }
        else
        {
            obj.reset();
        }
    }
};

namespace bitsery
{
namespace traits
{
template<class T>
struct ExtensionTraits<CustomUniquePtrExt, T>
{
    using TValue = typename T::element_type;
    static constexpr bool SupportValueOverload = true;
    static constexpr bool SupportObjectOverload = true;
    static constexpr bool SupportLambdaOverload = true;
};
}
}

namespace bitsery
{
template<typename S, size_t N>
void serialize(S& s, std::bitset<N>& obj)
{
    s.ext(obj, bitsery::ext::StdBitset{});
}
template<typename S, class T>
void serialize(S& s, std::unique_ptr<T>& obj)
{
    s.ext(obj, CustomUniquePtrExt{});
}
}

namespace absim
{

static bool bitsery_ok(bitsery::ReaderError error, bool completed)
{
    return error == bitsery::ReaderError::NoError && completed;
}

bool compress_zlib(std::vector<uint8_t>& dst, void const* src, size_t src_bytes)
{
    dst.resize(mz_compressBound((mz_ulong)src_bytes));
    mz_ulong dst_size = (mz_ulong)dst.size();
    if(MZ_OK != mz_compress2(
        dst.data(), &dst_size,
        (uint8_t const*)src, (mz_ulong)src_bytes,
        MZ_BEST_COMPRESSION))
    {
        dst.clear();
        return false;
    }
    dst.resize((size_t)dst_size);
    return true;
}

bool uncompress_zlib(std::vector<uint8_t>& dst, void const* src, size_t src_bytes)
{
    constexpr size_t BLOCK_SIZE = 1 << 16;
    dst.resize(BLOCK_SIZE);
    size_t block = 0;

    mz_stream stream{};
    stream.next_in = (unsigned char const*)src;
    stream.avail_in = (mz_uint32)src_bytes;
    stream.next_out = dst.data();
    stream.avail_out = (mz_uint32)BLOCK_SIZE;

    if(MZ_OK != mz_inflateInit(&stream))
        return false;

    for(;;)
    {
        int status = mz_inflate(&stream, MZ_NO_FLUSH);
        if(status == MZ_STREAM_END)
        {
            dst.resize(dst.size() - stream.avail_out);
            break;
        }
        if(status != MZ_OK)
        {
            mz_inflateEnd(&stream);
            dst.clear();
            return false;
        }
        size_t n = dst.size();
        dst.resize(n * 2);
        stream.next_out = dst.data() + n;
        stream.avail_out = (mz_uint32)n;
    }

    return MZ_OK == mz_inflateEnd(&stream);
}

static bool uncompress_zlib_sized(
    std::vector<uint8_t>& dst,
    void const* src,
    size_t src_bytes,
    size_t dst_bytes)
{
    dst.resize(dst_bytes);

    mz_ulong actual_dst_bytes = (mz_ulong)dst.size();
    mz_ulong actual_src_bytes = (mz_ulong)src_bytes;
    int status = mz_uncompress2(
        dst.data(), &actual_dst_bytes,
        (uint8_t const*)src, &actual_src_bytes);

    if(status != MZ_OK ||
        actual_dst_bytes != dst_bytes ||
        actual_src_bytes != src_bytes)
    {
        dst.clear();
        return false;
    }

    return true;
}

template<typename CharT>
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
        setg((char*)vec.data(), (char*)vec.data() + (size_t)pos, (char*)vec.data() + vec.size());
        return pos;
    }
};

template<class Archive>
static std::string serdes_savestate(Archive& ar, arduboy_t& a)
{
    ar(a.core_state.cpu.data);
    ar(a.core_state.cpu.active);
    ar(a.core_state.cpu.wakeup_cycles);
    ar(a.core_state.cpu.just_interrupted);
    ar(a.core_state.cpu.min_stack);
    ar(a.core_state.cpu.stack_check);
    ar(a.core_state.cpu.pushed_at_least_once);
    ar(a.core_state.cpu.eeprom);
    ar(a.core_state.cpu.eeprom_modified_bytes);
    ar(a.core_state.cpu.eeprom_modified);
    ar(a.core_state.cpu.eeprom_dirty);
    ar(a.core_state.cpu.prev_sreg);
    ar(a.core_state.cpu.pc);
    ar(a.core_state.cpu.executing_instr_pc);
    for(auto& f : a.core_state.cpu.stack_frames)
    {
        ar(f.cycle);
        ar(f.pc);
        ar(f.sp);
    }
    ar(a.core_state.cpu.num_stack_frames);
    ar(a.core_state.cpu.program_loaded);
    ar(a.core_state.cpu.timer_sync);
    ar(a.core_state.cpu.timer0);
    ar(a.core_state.cpu.timer1);
    ar(a.core_state.cpu.timer3);
    ar(a.core_state.cpu.timer4);

    ar(a.core_state.cpu.pll_prev_cycle);
    ar(a.core_state.cpu.pll_lock_cycle);
    ar(a.core_state.cpu.pll_num12);
    ar(a.core_state.cpu.pll_busy);

    ar(a.core_state.cpu.spsr_read_after_transmit);
    ar(a.core_state.cpu.spi_busy);
    ar(a.core_state.cpu.spi_busy_clear);
    ar(a.core_state.cpu.spi_latch_read);
    ar(a.core_state.cpu.spi_data_latched);
    ar(a.core_state.cpu.spi_data_byte);
    ar(a.core_state.cpu.spi_datain_byte);
    ar(a.core_state.cpu.spi_done_cycle);
    ar(a.core_state.cpu.spi_transmit_zero_cycle);
    ar(a.core_state.cpu.spi_clock_cycles);

    ar(a.core_state.cpu.twi_prev_cycle);
    ar(a.core_state.cpu.twi_done_cycle);
    ar(a.core_state.cpu.twi_mode);
    ar(a.core_state.cpu.twi_pending);
    ar(a.core_state.cpu.twi_status);
    ar(a.core_state.cpu.twi_address);
    ar(a.core_state.cpu.twi_busy);
    ar(a.core_state.cpu.twi_started);
    ar(a.core_state.cpu.twi_repeated_start);
    ar(a.core_state.cpu.twi_reading);
    ar(a.core_state.cpu.twi_general_call);
    ar(a.core_state.cpu.twi_pull_scl_low);
    ar(a.core_state.cpu.twi_pull_sda_low);
    ar(a.core_state.cpu.twi_external_scl_low);
    ar(a.core_state.cpu.twi_external_sda_low);

    ar(a.core_state.cpu.eeprom_prev_cycle);
    ar(a.core_state.cpu.eeprom_clear_eempe_cycles);
    ar(a.core_state.cpu.eeprom_write_addr);
    ar(a.core_state.cpu.eeprom_write_data);
    ar(a.core_state.cpu.eeprom_program_cycles);
    ar(a.core_state.cpu.eeprom_busy);

    ar(a.core_state.cpu.adc_prev_cycle);
    ar(a.core_state.cpu.adc_prescaler_cycle);
    ar(a.core_state.cpu.adc_cycle);
    ar(a.core_state.cpu.adc_ref);
    ar(a.core_state.cpu.adc_result);
    ar(a.core_state.cpu.adc_seed);
    ar(a.core_state.cpu.adc_busy);
    ar(a.core_state.cpu.adc_nondeterminism);

    ar(a.core_state.cpu.sound_prev_cycle);
    ar(a.core_state.cpu.sound_cycle);
    ar(a.core_state.cpu.sound_enabled);
    ar(a.core_state.cpu.sound_pwm);
    ar(a.core_state.cpu.sound_pwm_val);

    ar(a.core_state.cpu.usb_ep);
    ar(a.core_state.cpu.usb_next_sofi_cycle);
    ar(a.core_state.cpu.usb_next_eorsti_cycle);
    ar(a.core_state.cpu.usb_next_setconf_cycle);
    ar(a.core_state.cpu.usb_next_setlength_cycle);
    ar(a.core_state.cpu.usb_next_update_cycle);
    ar(a.core_state.cpu.usb_dpram);
    ar(a.core_state.cpu.usb_attached);

    ar(a.core_state.cpu.spm_prev_cycle);
    ar(a.core_state.cpu.spm_busy);
    ar(a.core_state.cpu.spm_en_cycles);
    ar(a.core_state.cpu.spm_op);
    ar(a.core_state.cpu.spm_cycles);
    ar(a.core_state.cpu.spm_buffer);

    ar(a.core_state.cpu.watchdog_divider);
    ar(a.core_state.cpu.watchdog_divider_cycle);
    ar(a.core_state.cpu.watchdog_prev_cycle);
    ar(a.core_state.cpu.watchdog_next_cycle);

    ar(a.core_state.cpu.peripheral_queue);

    ar(a.core_state.cpu.cycle_count);

    ar(a.core_state.cpu.lock);
    ar(a.core_state.cpu.fuse_lo);
    ar(a.core_state.cpu.fuse_hi);
    ar(a.core_state.cpu.fuse_ext);

    ar(a.peripherals.display.filtered_pixels);
    ar(a.peripherals.display.filtered_pixel_counts);
    ar(a.peripherals.display.type);
    ar(a.peripherals.display.pixels);
    ar(a.peripherals.display.pixel_history_index);
    ar(a.peripherals.display.enable_filter);
    ar(a.peripherals.display.ram);
    ar(a.peripherals.display.ref_segment_current);
    ar(a.peripherals.display.current_limit_slope);
    ar(a.peripherals.display.enable_current_limiting);
    ar(a.peripherals.display.prev_row_drive);
    ar(a.peripherals.display.contrast);
    ar(a.peripherals.display.entire_display_on);
    ar(a.peripherals.display.inverse_display);
    ar(a.peripherals.display.display_on);
    ar(a.peripherals.display.enable_charge_pump);
    ar(a.peripherals.display.addressing_mode);
    ar(a.peripherals.display.col_start);
    ar(a.peripherals.display.col_end);
    ar(a.peripherals.display.page_start);
    ar(a.peripherals.display.page_end);
    ar(a.peripherals.display.mux_ratio);
    ar(a.peripherals.display.display_offset);
    ar(a.peripherals.display.display_start);
    ar(a.peripherals.display.com_scan_direction);
    ar(a.peripherals.display.alternative_com);
    ar(a.peripherals.display.com_remap);
    ar(a.peripherals.display.segment_remap);
    ar(a.peripherals.display.fosc_index);
    ar(a.peripherals.display.divide_ratio);
    ar(a.peripherals.display.phase_1);
    ar(a.peripherals.display.phase_2);
    ar(a.peripherals.display.vcomh_deselect);
    ar(a.peripherals.display.row);
    ar(a.peripherals.display.row_cycle);
    ar(a.peripherals.display.cycles_per_row);
    ar(a.peripherals.display.ps_per_clk);
    ar(a.peripherals.display.ps_rem);
    ar(a.peripherals.display.processing_command);
    ar(a.peripherals.display.current_command);
    ar(a.peripherals.display.command_byte_index);
    ar(a.peripherals.display.data_page);
    ar(a.peripherals.display.data_col);
    ar(a.peripherals.display.vsync);

    ar(a.peripherals.fx.sectors_modified_data);
    ar(a.peripherals.fx.sectors_modified);
    ar(a.peripherals.fx.sectors_dirty);
    ar(a.peripherals.fx.enabled);
    ar(a.peripherals.fx.woken_up);
    ar(a.peripherals.fx.write_enabled);
    ar(a.peripherals.fx.reading_status);
    ar(a.peripherals.fx.processing_command);
    ar(a.peripherals.fx.reading);
    ar(a.peripherals.fx.programming);
    ar(a.peripherals.fx.erasing_sector);
    ar(a.peripherals.fx.releasing);
    ar(a.peripherals.fx.reading_jedec_id);
    ar(a.peripherals.fx.busy_ps_rem);
    ar(a.peripherals.fx.current_addr);
    ar(a.peripherals.fx.command);
    ar(a.peripherals.fx.min_page);
    ar(a.peripherals.fx.max_page);
    ar(a.peripherals.fx.busy_error);

    ar(a.peripherals.fxport_reg);
    ar(a.peripherals.fxport_mask);
    ar(a.program_state.game_hash);
    ar(a.program_state.title);
    ar(a.program_state.device_type);
    ar(a.peripherals.prev_display_reset);
    ar(a.core_state.ps_rem);

    return "";
}

template<bool is_load, class Archive>
static std::string serdes_snapshot(Archive& ar, arduboy_t& a)
{
    ar(a.program_state.prog_filename);
    ar(a.program_state.prog_filedata);

    if(is_load)
    {
        vectorwrapbuf<uint8_t> vb(a.program_state.prog_filedata);
        std::istream is(&vb);
        auto r = a.load_file(a.program_state.prog_filename.c_str(), is);
        if(!r.empty())
            return r;
    }

    ar(a.core_state.cpu.serial_bytes);
    ar(a.core_state.cpu.sound_buffer);
    ar(a.peripherals.fx.sectors);

    ar(a.profiler_state.hotspots_symbol);
    ar(a.profiler_state.total);
    ar(a.profiler_state.total_with_sleep);
    ar(a.profiler_state.prev_frame_cycles);
    ar(a.profiler_state.total_frames);
    ar(a.profiler_state.prev_ms_cycles);
    ar(a.profiler_state.total_ms);
    ar(a.profiler_state.frame_bytes_total);
    ar(a.profiler_state.frame_bytes);
    ar(a.profiler_state.frame_cpu_usage);
    ar(a.profiler_state.ms_cpu_usage_raw);
    ar(a.profiler_state.ms_cpu_usage);
    ar(a.profiler_state.enabled);
    ar(a.profiler_state.cached_total);
    ar(a.profiler_state.cached_total_with_sleep);
    ar(a.profiler_state.hotspots);
    ar(a.profiler_state.num_hotspots);

    ar(a.debugger_state.breakpoints);
    ar(a.debugger_state.breakpoints_rd);
    ar(a.debugger_state.breakpoints_wr);
    ar(a.debugger_state.paused);

    ar(a.program_state.cfg.display_type);
    ar(a.program_state.cfg.fxport_reg);
    ar(a.program_state.cfg.fxport_mask);
    ar(a.program_state.cfg.bootloader);
    ar(a.program_state.cfg.boot_to_menu);
    ar(a.program_state.flashcart_loaded);

    serdes_savestate(ar, a);

    // time-travel debugging history
    ar(a.debugger_state.input_history);
    ar(a.debugger_state.state_history);
    ar(a.debugger_state.present_state);
    ar(a.debugger_state.present_cycle);

    return "";
}

std::string arduboy_t::save_savestate(std::ostream& f)
{
    bitsery::Serializer<bitsery::OutputStreamAdapter> ar(f);
    ar(SNAPSHOT_ID);
    ar(SNAPSHOT_VERSION);
    return serdes_savestate(ar, *this);
}

size_t arduboy_t::max_savestate_size() const
{
    std::ostringstream ss;
    const_cast<arduboy_t*>(this)->save_savestate(ss);
    size_t size = ss.str().size();

    // Calculate the theoretical maximum serialize size:
    // if all FX sectors are present (each sector is 4097 bytes serialized).
    size_t num_sectors = 0;
    for(auto const& s : peripherals.fx.sectors)
        if(s) ++num_sectors;
    num_sectors = std::min<size_t>(num_sectors, w25q128_t::NUM_SECTORS);
    size += (w25q128_t::NUM_SECTORS - num_sectors) * 4097;

    return size;
}

std::string arduboy_t::load_savestate(std::istream& f)
{
    bitsery::Deserializer<bitsery::InputStreamAdapter> ar(f);

    std::array<char, 8> id{};
    ar(id);
    if(ar.adapter().error() != bitsery::ReaderError::NoError)
        return "Snapshot: invalid header";
    if(id != SNAPSHOT_ID)
        return "Snapshot: invalid identifier";

    version_t version{};
    ar(version);
    if(ar.adapter().error() != bitsery::ReaderError::NoError)
        return "Snapshot: invalid header";
    if(version != SNAPSHOT_VERSION)
        return "Snapshot: incompatible version (created with " + version_str(version) + ")";

    std::vector<uint8_t> backup;
    {
        bitsery::Serializer<bitsery::OutputBufferAdapter<std::vector<uint8_t>>> bak(backup);
        auto br = serdes_savestate(bak, *this);
        if(!br.empty()) return br;
    }

    auto r = serdes_savestate(ar, *this);
    if(!r.empty())
    {
        bitsery::Deserializer<bitsery::InputBufferAdapter<std::vector<uint8_t>>> bak(
            backup.begin(), backup.end());
        serdes_savestate(bak, *this);
        return r;
    }
    if(ar.adapter().error() != bitsery::ReaderError::NoError)
    {
        bitsery::Deserializer<bitsery::InputBufferAdapter<std::vector<uint8_t>>> bak(
            backup.begin(), backup.end());
        serdes_savestate(bak, *this);
        return "Snapshot: invalid data";
    }
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
        data.resize(ar.adapter().writtenBytesCount());

#if 0
        // time-travel state
        ar(input_history);
        if(state_history.empty())
            ar(state_history);
        else
        {
            std::vector<tt_state_t> t;
            t.push_back(state_history.front());
            ar(t);
        }
        ar(present_state);
        ar(present_cycle);
#endif
    }

    // compress
    std::vector<uint8_t> dst;
    if(!compress_zlib(dst, data.data(), data.size()))
        return false;

    {
        bitsery::Serializer<StreamAdapter> ar(f);
        ar(SNAPSHOT_ID);
        ar(SNAPSHOT_VERSION);
        uint32_t tsize = (uint32_t)data.size();
        ar(tsize);
    }
    f.write((char const*)dst.data(), dst.size());
    return true;
}

std::string arduboy_t::load_snapshot(std::istream& f)
{
    using Buffer = std::vector<uint8_t>;
    using BufferAdapter = bitsery::InputBufferAdapter<Buffer>;
    using StreamAdapter = bitsery::InputStreamAdapter;

    // uncompress

    std::istringstream ss;
    uint32_t dst_size = 0;

    {
        bitsery::Deserializer<StreamAdapter> ar(f);

        std::array<char, 8> id{};
        ar(id);
        if(ar.adapter().error() != bitsery::ReaderError::NoError)
            return "Snapshot: invalid header";
        if(id != SNAPSHOT_ID)
            return "Snapshot: invalid identifier";

        version_t version{};
        ar(version);
        if(ar.adapter().error() != bitsery::ReaderError::NoError)
            return "Snapshot: invalid header";
        if(version != SNAPSHOT_VERSION)
            return "Snapshot: incompatible version (created with " + version_str(version) + ")";

        ar(dst_size);
        if(ar.adapter().error() != bitsery::ReaderError::NoError)
            return "Snapshot: invalid header";

        if(dst_size >= 256 * 1024 * 1024)
            return "Snapshot: data too large";
    }

    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());

    std::vector<uint8_t> dst;
    if(!uncompress_zlib_sized(dst, data.data(), data.size(), dst_size))
        return "Snapshot: invalid data";

    // deserialize
    {
        std::vector<uint8_t> backup;
        {
            bitsery::Serializer<bitsery::OutputBufferAdapter<std::vector<uint8_t>>> bak(backup);
            auto br = serdes_snapshot<false>(bak, *this);
            if(!br.empty()) return br;
        }

        bitsery::Deserializer<BufferAdapter> ar(dst.begin(), dst.end());
        core_state.cpu.reset();
        auto r = serdes_snapshot<true>(ar, *this);
        if(!r.empty())
        {
            bitsery::Deserializer<BufferAdapter> bak(backup.begin(), backup.end());
            serdes_snapshot<true>(bak, *this);
            return r;
        }
        if(!bitsery_ok(ar.adapter().error(), ar.adapter().isCompletedSuccessfully()))
        {
            bitsery::Deserializer<BufferAdapter> bak(backup.begin(), backup.end());
            serdes_snapshot<true>(bak, *this);
            return "Snapshot: invalid data";
        }

#if 0
        // time-travel state
        ar(input_history);
        ar(state_history);
        ar(present_state);
        ar(present_cycle);

        // resimulate history
        if(!state_history.empty())
        {
            uint64_t saved_present_cycle = std::max(cpu.cycle_count, present_cycle);
            auto saved_present_state = present_state;
            uint64_t saved_cycle = cpu.cycle_count;
            bool saved_paused = paused;
            state_history.resize(1);
            travel_to_present();
            travel_back_to_cycle(state_history.back().cycle);
            paused = false;

            while(state_history.back().cycle + STATE_HISTORY_CYCLES < saved_present_cycle)
            {
                ps_rem = PS_BUFFER;
                advance(CYCLE_PS * STATE_HISTORY_CYCLES);
                tt_state_t state;
                state.cycle = cpu.cycle_count;
                save_state_to_vector(state.state);
                state_history.emplace_back(std::move(state));
            }
            travel_to_present();
            travel_back_to_cycle(saved_cycle);
            present_cycle = saved_present_cycle;
            present_state = saved_present_state;
            paused = saved_paused;
        }
#endif
    }

    return "";
}

void arduboy_t::save_savedata(std::ostream& f)
{
    using StreamAdapter = bitsery::OutputStreamAdapter;
    bitsery::Serializer<StreamAdapter> ar(f);
    save_data_state.savedata.game_hash = program_state.game_hash;
    ar(save_data_state.savedata);
}

bool arduboy_t::load_savedata(std::istream& f)
{
    using StreamAdapter = bitsery::InputStreamAdapter;
    bitsery::Deserializer<StreamAdapter> ar(f);
    savedata_t data;
    data.clear();
    ar(data);
    if(!bitsery_ok(ar.adapter().error(), ar.adapter().isCompletedSuccessfully()))
        return false;
    if(data.game_hash != program_state.game_hash)
    {
        return false;
    }
    save_data_state.savedata = std::move(data);

    // overwrite eeprom / fx with saved data

    auto const& d = save_data_state.savedata;
    if(d.eeprom.size() == core_state.cpu.eeprom.size())
        memcpy(core_state.cpu.eeprom.data(), d.eeprom.data(), array_bytes(core_state.cpu.eeprom));

    for(auto const& kv : d.fx_sectors)
    {
        uint32_t sector = kv.first;
        auto const& sdata = kv.second;
        if(sector >= peripherals.fx.NUM_SECTORS) continue;
        peripherals.fx.write_bytes(sector * 4096, sdata.data(), 4096);
    }

    return true;
}

}
