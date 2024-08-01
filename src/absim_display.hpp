#include "absim.hpp"

#include <cmath>

#if defined(ARDENS_SSE2)
#include <emmintrin.h>
#endif

namespace absim
{

void display_t::send_command(uint8_t byte)
{
    if(!processing_command)
    {
        command_byte_index = 0;
        current_command = byte;
        processing_command = true;
    }
    switch(current_command)
    {
    case 0x00: case 0x01: case 0x02: case 0x03:
    case 0x04: case 0x05: case 0x06: case 0x07:
    case 0x08: case 0x09: case 0x0a: case 0x0b:
    case 0x0c: case 0x0d: case 0x0e: case 0x0f:
        data_col &= 0xf0;
        data_col |= current_command;
        processing_command = false;
        break;

    case 0x10: case 0x11: case 0x12: case 0x13:
    case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x18: case 0x19: case 0x1a: case 0x1b:
    case 0x1c: case 0x1d: case 0x1e: case 0x1f:
        data_col &= 0x0f;
        data_col |= current_command << 4;
        if(type != SH1106)
            data_col &= 0x7f;
        processing_command = false;
        break;

    case 0x20:
        if(command_byte_index == 1)
        {
            switch(byte & 0x3)
            {
            case 0: addressing_mode = addr_mode::HORIZONTAL; break;
            case 1: addressing_mode = addr_mode::VERTICAL;   break;
            case 2: addressing_mode = addr_mode::PAGE;       break;
            case 3:                                          break;
            default: break;
            }
            processing_command = false;
        }
        break;

    case 0x21:
        if(command_byte_index == 1)
        {
            col_start = byte & 0x7f;
            data_col = col_start;
        }
        if(command_byte_index == 2)
        {
            col_end = byte & 0x7f;
            processing_command = false;
        }
        break;

    case 0x22:
        if(command_byte_index == 1)
        {
            page_start = byte & 0x7;
            data_page = page_start;
        }
        if(command_byte_index == 2)
        {
            page_end = byte & 0x7;
            processing_command = false;
        }
        break;

    case 0x26:
    case 0x27:
        // TODO: continuous horizontal scroll setup
        if(command_byte_index == 6)
            processing_command = false;
        break;

    case 0x29:
    case 0x2a:
        // TODO: continuous vertical and horizontal scroll setup
        if(command_byte_index == 5)
            processing_command = false;
        break;

    case 0x2e:
        // TODO: deactivate scroll
        processing_command = false;
        break;

    case 0x2f:
        // TODO: activate scroll
        processing_command = false;
        break;

    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
        if(type == SH1106)
        {
            // TODO: set pump voltage value
            // 0: VPP = 6.4 V
            // 1: VPP = 7.4 V
            // 2: VPP = 8.0 V (power on)
            // 3: VPP = 9.0 V
        }
        processing_command = false;
        break;

    case 0x81:
        if(command_byte_index == 1)
        {
            contrast = byte;
            processing_command = false;
        }
        break;

    case 0x8d:
        if(type == SSD1309)
        {
            processing_command = false;
            break;
        }
        if(command_byte_index == 1)
        {
            enable_charge_pump = (byte == 0x14);
            processing_command = false;
        }
        break;

    case 0xb0: case 0xb1 : case 0xb2 : case 0xb3:
    case 0xb4: case 0xb5 : case 0xb6 : case 0xb7:
        data_page = byte & 0x7;
        processing_command = false;
        break;

    case 0xa0:
        segment_remap = false;
        processing_command = false;
        break;

    case 0xa1:
        segment_remap = true;
        processing_command = false;
        break;

    case 0xa4:
        entire_display_on = true;
        processing_command = false;
        break;

    case 0xa5:
        entire_display_on = false;
        processing_command = false;
        break;

    case 0xa6:
        inverse_display = false;
        processing_command = false;
        break;

    case 0xa7:
        inverse_display = true;
        processing_command = false;
        break;

    case 0xa8:
        if(command_byte_index == 1)
        {
            mux_ratio = byte & 0x3f;
            processing_command = false;
        }
        break;

    case 0xae:
        display_on = false;
        processing_command = false;
        break;

    case 0xaf:
        display_on = true;
        processing_command = false;
        break;

    case 0xc0:
        com_scan_direction = false;
        processing_command = false;
        break;

    case 0xc8:
        com_scan_direction = true;
        processing_command = false;
        break;

    case 0xd3:
        if(command_byte_index == 1)
        {
            display_offset = byte & 0x3f;
            processing_command = false;
        }
        break;

    case 0xd5:
        if(command_byte_index == 1)
        {
            divide_ratio = byte & 0xf;
            fosc_index = byte >> 4;
            update_clocking();
            processing_command = false;
        }
        break;

    case 0xda:
        if(command_byte_index == 1)
        {
            alternative_com = (byte & 0x10) != 0;
            com_remap = (byte & 0x20) != 0;
            processing_command = false;
        }
        break;

    case 0xd9:
        if(command_byte_index == 1)
        {
            phase_1 = byte & 0xf;
            phase_2 = byte >> 4;
            processing_command = false;
        }
        break;

    case 0xdb:
        if(command_byte_index == 1)
        {
            vcomh_deselect = (byte >> 4) & 0x7;
            processing_command = false;
        }
        break;

    case 0xe3:
        processing_command = false;
        break;

    default:

        if(current_command >= 0x40 && current_command < 0x80)
        {
            display_start = current_command & 0x3f;
            processing_command = false;
            break;
        }

        processing_command = false;
        break;
    }
    ++command_byte_index;
}

void display_t::send_data(uint8_t byte)
{
    if(type != SH1106 || uint8_t(data_col - 2) < 128)
    {
        uint8_t col = data_col;
        if(type == SH1106)
            col -= 2;
        uint8_t mapped_col = segment_remap ? 127 - col : col;
        size_t i = data_page * 128 + mapped_col;
        ram[i & 1023] = byte;
    }

    switch(addressing_mode)
    {
    case addr_mode::HORIZONTAL:
        if(data_col >= col_end)
        {
            data_col = col_start;
            if(data_page >= page_end)
                data_page = page_start;
            else
                data_page = (data_page + 1) & 0x7;
        }
        else
            data_col = (data_col + 1) & 0x7f;
        break;

    case addr_mode::VERTICAL:
        if(data_page >= page_end)
        {
            data_page = page_start;
            if(data_col >= col_end)
                data_col = col_start;
            else
                data_col = (data_col + 1) & 0x7f;
        }
        else
            data_page = (data_page + 1) & 0x7;
        break;
        
    case addr_mode::PAGE:
        if(data_col >= col_end)
            data_col = col_start;
        else
            data_col = (data_col + 1) & 0x7f;
        break;

    default:
        break;
    }
}

ARDENS_FORCEINLINE void display_t::update_pixels_row()
{
    uint8_t ram_row = row;
    ram_row += display_start;
    ram_row &= 63;

    uint8_t mask = 1 << (ram_row % 8);
    size_t rindex = (ram_row / 8) * 128;

    uint8_t out_row = row;
    out_row -= display_offset;
    if(com_scan_direction) out_row = mux_ratio - out_row;
    out_row &= 63;

    // the Arduboy's display is upside-down
    size_t pindex = (63 - out_row) * 128 + 128;

    if(!enable_filter)
        pixel_history_index = 0;

    auto& parray = pixels[pixel_history_index];

    if((mux_ratio >= 16 && row == mux_ratio) || row >= 63)
    {
        if(enable_filter && ++pixel_history_index >= 4)
            pixel_history_index = 0;
        vsync = true;
    }

    constexpr float F = 0.65f;
    constexpr uint32_t FA = uint32_t(F * 256);
    constexpr uint32_t FB = 280 - FA;
    uint8_t tcontrast = (uint8_t)std::min<uint32_t>(255,
        contrast == 0 ? 0 : FA + contrast * FB / 256);
    uint8_t p0 = 0;
    uint8_t p1 = enable_charge_pump ? tcontrast : tcontrast >> 4;

    // current limiting
    if(enable_current_limiting)
    {
        // count number of pixels on in the current row
        int num_pixels_on = 0;
        for(int i = 0; i < 128; ++i)
            if(ram[rindex + i] & mask)
                ++num_pixels_on;
        if(inverse_display)
            num_pixels_on = 128 - num_pixels_on;

        float row_drive =
            ref_segment_current * (1.f / 255) * num_pixels_on * tcontrast;

        constexpr float DIFF = MAX_DRIVER_CURRENT * 0.5f;
        if(row != 0 && std::abs(row_drive - prev_row_drive) > DIFF)
        {
            constexpr float F = 0.35f;
            if(row_drive > prev_row_drive)
                row_drive = F * (prev_row_drive + DIFF) + (1.f - F) * row_drive;
            else
                row_drive = F * (prev_row_drive - DIFF) + (1.f - F) * row_drive;
        }

        if(row_drive > MAX_DRIVER_CURRENT)
        {
            float t = MAX_DRIVER_CURRENT / row_drive;
            t += current_limit_slope * (1.f - t);
            row_drive *= t;
        }

        if(num_pixels_on > 0)
        {
            float segment_drive = row_drive / num_pixels_on;
            float t = segment_drive / ref_segment_current * 255;
            if(t > tcontrast)
                t = float(tcontrast);
            p1 = uint8_t(t);
        }

        prev_row_drive = row_drive;
    }

    if(inverse_display) std::swap(p0, p1);

    for(int i = 0; i < 128; ++i)
    {
        uint8_t p = p0;
        if(ram[rindex++] & mask)
            p = p1;
        // decrement because the Arduboy's display is upside-down
        parray[--pindex] = p;
    }
    if(vsync && enable_filter)
        filter_pixels();
}

void display_t::filter_pixels()
{
    memset(&filtered_pixel_counts, 0, sizeof(filtered_pixel_counts));
    static constexpr uint8_t C[4] = { 42, 84, 84, 42 };
    for(int n = 0; n < 4; ++n)
    {
        uint8_t c = C[(7 - n + pixel_history_index) % 4];
        uint8_t const* src = &pixels[n][0];
        uint16_t* dst = &filtered_pixel_counts[0];
        uint16_t* dst_end = dst + 8192;
#if defined(ARDENS_SSE2)
        __m128i vc = _mm_set1_epi16((int16_t)c);
        __m128i vz = _mm_setzero_si128();
        while(dst < dst_end)
        {
            __m128i vd0 = _mm_loadu_si128((__m128i const*)dst + 0);
            __m128i vd1 = _mm_loadu_si128((__m128i const*)dst + 1);
            __m128i vs = _mm_loadu_si128((__m128i const*)src);
            __m128i vr0 = _mm_add_epi16(vd0, _mm_mullo_epi16(_mm_unpacklo_epi8(vs, vz), vc));
            __m128i vr1 = _mm_add_epi16(vd1, _mm_mullo_epi16(_mm_unpackhi_epi8(vs, vz), vc));
            _mm_storeu_si128((__m128i*)dst + 0, vr0);
            _mm_storeu_si128((__m128i*)dst + 1, vr1);
            src += 16;
            dst += 16;
        }
#else
        while(dst < dst_end)
            *dst++ += *src++ * c;
#endif
    }
#if defined(ARDENS_SSE2)
    {
        uint16_t const* src = &filtered_pixel_counts[0];
        uint8_t* dst = &filtered_pixels[0];
        uint8_t* dst_end = dst + 8192;
        while(dst < dst_end)
        {
            // vs0: 0.1.2.3.4.5.6.7.
            // vs0: 8.9.A.B.C.D.E.F.
            __m128i vs0 = _mm_loadu_si128((__m128i const*)src + 0);
            __m128i vs1 = _mm_loadu_si128((__m128i const*)src + 1);
            __m128i vt0 = _mm_srli_epi16(vs0, 8);
            __m128i vt1 = _mm_srli_epi16(vs1, 8);
            __m128i vd = _mm_packus_epi16(vt0, vt1);
            _mm_storeu_si128((__m128i*)dst, vd);
            src += 16;
            dst += 16;
        }
    }
#else
    for(int i = 0; i < 8192; ++i)
        filtered_pixels[i] = uint8_t(filtered_pixel_counts[i] / 256);
#endif
}

ARDENS_FORCEINLINE bool display_t::advance(uint64_t ps)
{
    vsync = false;
    ps += ps_rem;
    ps_rem = 0;

    if(ps < ps_per_clk)
    {
        ps_rem = ps;
        return false;
    }

    uint32_t rc = row_cycle + uint32_t(ps / ps_per_clk);
    ps_rem = ps % ps_per_clk;
    while(rc >= cycles_per_row)
    {
        update_pixels_row();
        if(row == mux_ratio)
            row = 0;
        else
            row = (row + 1) % 64;
        rc -= cycles_per_row;
    }
    row_cycle = (uint8_t)rc;

    return vsync;
}

constexpr std::array<double, 16> FOSC =
{
    // mostly made up
    200, 224, 248, 272, 296, 320, 344, 368,
    392, 416, 440, 464, 488, 512, 536, 570,
};

ARDENS_FORCEINLINE void display_t::update_clocking()
{
    cycles_per_row = phase_1 + phase_2 + 50;
    ps_per_clk = (uint64_t)round(1e12 * (divide_ratio + 1) / fosc());
}

void display_t::reset()
{
    memset(&ram, 0, sizeof(ram));
    memset(&pixels, 0, sizeof(pixels));
    memset(&filtered_pixels, 0, sizeof(filtered_pixels));

    ref_segment_current = 0.195f;
    current_limit_slope = 0.75f;
    enable_current_limiting = true;
    prev_row_drive = 0;

    contrast = 0x7f;
    entire_display_on = false;
    inverse_display = false;
    display_on = false;
    enable_charge_pump = false;

    addressing_mode = addr_mode::PAGE;

    col_start = 0;
    col_end = (type == SH1106? 131 : 127);
    page_start = 0;
    page_end = 7;

    mux_ratio = 63;

    display_offset = 0;
    display_start = 0;

    com_scan_direction = false;
    alternative_com = true;
    com_remap = false;
    segment_remap = false;

    fosc_index = 8;
    divide_ratio = 0;
    phase_1 = 2;
    phase_2 = 2;
    vcomh_deselect = 2;

    row = 0;
    row_cycle = 0;
    cycles_per_row = 0;
    ps_per_clk = 0;

    ps_rem = 0;
    
    processing_command = false;
    current_command = 0;
    command_byte_index = 0;

    data_page = 0;
    data_col = 0;

    update_clocking();
}

double display_t::fosc() const
{
    double f = FOSC[fosc_index % 16] * 1000.0;
    if(type == SH1106)
        f *= 0.8f;
    return f;
}

double display_t::refresh_rate() const
{
    int D = divide_ratio + 1;
    int K = phase_1 + phase_2 + 50;
    int MUX = mux_ratio + 1;
    return fosc() / double(D * K * MUX);
}

}
