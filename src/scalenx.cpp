#include "common.hpp"

#ifndef ARDENS_NO_SCALING
#include <hqx/HQ2x.hh>
#include <hqx/HQ3x.hh>
#endif

// used for scale4x first stage
static uint8_t tmpbuf[128 * 64 * 2 * 2];

int display_texture_zoom = -1;

// palette handling
void palette_rgba(int palette, uint8_t x, uint8_t y[4])
{
    switch(palette)
    {
    case PALETTE_RETRO:
    {
        constexpr uint32_t P0 = 0x071820;
        constexpr uint32_t P1 = 0x336856;
        constexpr uint32_t P2 = 0x88c170;
        constexpr uint32_t P3 = 0xe1f8d0;
        for(int i = 0; i < 3; ++i)
        {
            uint32_t t;
            uint32_t p0 = uint8_t(P0 >> (8 * i));
            uint32_t p1 = uint8_t(P1 >> (8 * i));
            uint32_t p2 = uint8_t(P2 >> (8 * i));
            uint32_t p3 = uint8_t(P3 >> (8 * i));
            if(x <= 0x55)
                t = p0 + (p1 - p0) * (x - 0x00) / 0x55;
            else if(x <= 0xaa)
                t = p1 + (p2 - p1) * (x - 0x55) / 0x55;
            else
                t = p2 + (p3 - p2) * (x - 0xaa) / 0x55;
            y[2 - i] = uint8_t(t);
        }
        break;
    }
    case PALETTE_LOW_CONTRAST:
        y[0] = y[1] = y[2] = uint8_t(16 + x * 192 / 256);
        break;
    case PALETTE_HIGH_CONTRAST:
    {
        uint8_t t = x < 128 ? uint8_t(x * 192u / 128u) : 255u - (255u - x) / 2u;
        y[0] = y[1] = y[2] = t;
        break;
    }
    case PALETTE_DEFAULT:
    default:
        y[0] = y[1] = y[2] = x;
        break;
    }
    y[3] = 255;
}

int filter_zoom(int f)
{
#ifndef ARDENS_NO_SCALING
    switch(f)
    {
    case FILTER_NONE:    return 1;
    case FILTER_SCALE2X: return 2;
    case FILTER_SCALE3X: return 3;
    case FILTER_SCALE4X: return 4;
    case FILTER_HQ2X:    return 2;
    case FILTER_HQ3X:    return 3;
    case FILTER_HQ4X:    return 4;
    default:             return 1;
    }
#else
    return 1;
#endif
}

int display_filter_zoom()
{
    int z = filter_zoom(settings.display_filtering);
    int d = settings.display_downsample;
    if(z % d == 0)
        z /= d;
    return z;
}

int recording_filter_zoom()
{
    int d = settings.recording_downsample;
    int z = filter_zoom(settings.recording_filtering);
    if(z % d == 0)
        z /= d;
    z *= settings.recording_zoom;
    return z;
}

void recreate_display_texture()
{
    int z = display_filter_zoom();
    if(z == display_texture_zoom)
        return;

    display_texture_zoom = z;

    platform_destroy_texture(display_texture);

    display_texture = platform_create_texture(128 * z, 64 * z);
}

#ifndef ARDENS_NO_SCALING
static void scale2x(uint8_t* dst, uint8_t const* src, int wd, int ht)
{
    for(int ni = 0; ni < ht; ++ni)
    {
        for(int nj = 0; nj < wd; ++nj)
        {
            int np = ni * wd + nj;
            uint8_t e = src[np];
            uint8_t b = (ni ==      0 ? 0 : src[np - wd]);
            uint8_t h = (ni == ht - 1 ? 0 : src[np + wd]);
            uint8_t d = (nj ==      0 ? 0 : src[np -  1]);
            uint8_t f = (nj == wd - 1 ? 0 : src[np +  1]);

            uint8_t e0, e1, e2, e3;
            if(b != h && d != f)
            {
                e0 = (d == b ? d : e);
                e1 = (b == f ? f : e);
                e2 = (d == h ? d : e);
                e3 = (h == f ? f : e);
            }
            else
            {
                e0 = e;
                e1 = e;
                e2 = e;
                e3 = e;
            }

            np = (ni * 2 * wd * 2) + (nj * 2);
            dst[np + 0] = e0;
            dst[np + 1] = e1;
            dst[np + wd * 2 + 0] = e2;
            dst[np + wd * 2 + 1] = e3;
        }
    }
}
#endif

#ifndef ARDENS_NO_SCALING
static void scale3x(uint8_t* dst, uint8_t const* src, int wd, int ht)
{
    for(int ni = 0; ni < ht; ++ni)
    {
        for(int nj = 0; nj < wd; ++nj)
        {
            int np = ni * wd + nj;
            uint8_t e = src[np];
            uint8_t b = (ni ==      0 ? 0 : src[np - wd]);
            uint8_t h = (ni == ht - 1 ? 0 : src[np + wd]);
            uint8_t d = (nj ==      0 ? 0 : src[np -  1]);
            uint8_t f = (nj == wd - 1 ? 0 : src[np +  1]);

            uint8_t a = (ni == 0 ? 0 : nj == 0 ? 0 : src[np - wd - 1]);
            uint8_t c = (ni == 0 ? 0 : nj == wd - 1 ? 0 : src[np - wd + 1]);
            uint8_t g = (ni == ht - 1 ? 0 : nj == 0 ? 0 : src[np + wd - 1]);
            uint8_t i = (ni == ht - 1 ? 0 : nj == wd - 1 ? 0 : src[np + wd + 1]);

            uint8_t e0, e1, e2, e3, e4, e5, e6, e7, e8;
            if(b != h && d != f)
            {
                e0 = d == b ? d : e;
                e1 = (d == b && e != c) || (b == f && e != a) ? b : e;
                e2 = b == f ? f : e;
                e3 = (d == b && e != g) || (d == h && e != a) ? d : e;
                e4 = e;
                e5 = (b == f && e != i) || (h == f && e != c) ? f : e;
                e6 = d == h ? d : e;
                e7 = (d == h && e != i) || (h == f && e != g) ? h : e;
                e8 = h == f ? f : e;
            }
            else
            {
                e0 = e1 = e2 = e;
                e3 = e4 = e5 = e;
                e6 = e7 = e8 = e;
            }

            np = (ni * 3 * wd * 3) + (nj * 3);
            dst[np + 0] = e0;
            dst[np + 1] = e1;
            dst[np + 2] = e2;
            np += wd * 3;
            dst[np + 0] = e3;
            dst[np + 1] = e4;
            dst[np + 2] = e5;
            np += wd * 3;
            dst[np + 0] = e6;
            dst[np + 1] = e7;
            dst[np + 2] = e8;
        }
    }
}
#endif

#ifndef ARDENS_NO_SCALING

static uint32_t hqbuf_src[128 * 64 * 2 * 2];
static uint32_t hqbuf_dst[128 * 64 * 4 * 4];

static void hqx_convert_src(uint8_t const* src, int n)
{
    assert(n <= sizeof(hqbuf_src) * sizeof(uint32_t));
    for(int i = 0; i < n; ++i)
    {
        uint32_t t = src[i];
        hqbuf_src[i] = t | (t << 8) | (t << 16) | 0xff000000;
    }
}

static void hqx_convert_dst(uint8_t* dst, int n)
{
    assert(n <= sizeof(hqbuf_dst) * sizeof(uint32_t));
    for(int i = 0; i < n; ++i)
        dst[i] = (uint8_t)hqbuf_dst[i];
}

static void hq2x(uint8_t* dst, uint8_t const* src, int wd, int ht)
{
    hqx_convert_src(src, wd * ht);

    HQ2x h;
    h.resize(hqbuf_src, (uint32_t)wd, (uint32_t)ht, hqbuf_dst);

    hqx_convert_dst(dst, wd * ht * 4);
}

static void hq3x(uint8_t* dst, uint8_t const* src, int wd, int ht)
{
    hqx_convert_src(src, wd * ht);

    HQ3x h;
    h.resize(hqbuf_src, (uint32_t)wd, (uint32_t)ht, hqbuf_dst);

    hqx_convert_dst(dst, wd * ht * 9);
}

#endif

static void scalenx_filter(int f, int d, uint8_t* dst, uint8_t const* src, bool rgba, int palette)
{
    static uint8_t downbuf[128 * 64 * 4 * 4];
    int z = filter_zoom(f);

#ifdef ARDENS_NO_SCALING
    uint8_t* tdst = rgba ? downbuf : dst;
    memcpy(tdst, src, 128 * 64);
#else

    if(!dst || !src) return;

    if(z % d != 0) d = 1;

    uint8_t* tdst = (d != 1 || rgba ? downbuf : dst);

    switch(f)
    {
    case FILTER_NONE:
        memcpy(tdst, src, 128 * 64);
        break;
    case FILTER_SCALE2X:
        scale2x(tdst, src, 128, 64);
        break;
    case FILTER_SCALE3X:
        scale3x(tdst, src, 128, 64);
        break;
    case FILTER_SCALE4X:
        scale2x(tmpbuf, src, 128, 64);
        scale2x(tdst, tmpbuf, 256, 128);
        break;
    case FILTER_HQ2X:
        hq2x(tdst, src, 128, 64);
        break;
    case FILTER_HQ3X:
        hq3x(tdst, src, 128, 64);
        break;
    case FILTER_HQ4X:
        hq2x(tmpbuf, src, 128, 64);
        hq2x(tdst, tmpbuf, 256, 128);
        break;
    default:
        break;
    }
#endif

    if(d != 1 || rgba)
    {
        int zd = z / d;
        int d2 = d * d;
        // dowsample from downbuf to dst
        for(int i = 0; i < 64 * zd; ++i)
        {
            for(int j = 0; j < 128 * zd; ++j)
            {
                int t = 0;
                for(int m = 0; m < d; ++m)
                    for(int n = 0; n < d; ++n)
                        t += tdst[(i * d + m) * 128 * z + j * d + n];
                uint8_t p = t / d2;
                int di = i * 128 * zd + j;
                if(rgba)
                    palette_rgba(palette, p, &dst[di * 4]);
                else
                    dst[di] = p;
            }
        }
    }
}

void scalenx(uint8_t* dst, uint8_t const* src, bool rgba)
{
    scalenx_filter(
        settings.display_filtering,
        settings.display_downsample,
        dst, src,
        rgba,
        settings.display_palette);
}


uint8_t* recording_pixels(bool rgba)
{
    static std::vector<uint8_t> pixels;
    static uint8_t tmp[128 * 64 * 4 * 4];
    uint8_t const* src = arduboy->display.filtered_pixels.data();

    int z = filter_zoom(settings.recording_filtering);
    int w = 128 * z;
    int h = 64 * z;
    int rz = settings.recording_zoom;

    pixels.resize(w * h * rz * rz * (rgba ? 4 : 1));

    scalenx_filter(
        settings.recording_filtering,
        settings.recording_downsample,
        tmp, src,
        false,
        settings.recording_palette);

    // adjust orientation here
    switch(settings.recording_orientation)
    {
    case 0:
        break;
    case 1:
    {
        std::vector<uint8_t> tmp2(w * h);
        for(int i = 0; i < h; ++i)
            for(int j = 0; j < w; ++j)
            {
                int ti = j;
                int tj = h - 1 - i;
                tmp2[ti * h + tj] = tmp[i * w + j];
            }
        memcpy(tmp, tmp2.data(), sizeof(uint8_t) * w * h);
        std::swap(w, h);
        break;
    }
    case 2:
        for(int i = 0; i < h / 2; ++i)
            for(int j = 0; j < w; ++j)
                std::swap(tmp[i * w + j], tmp[(h - 1 - i) * w + (w - 1 - j)]);
        break;
    case 3:
    {
        std::vector<uint8_t> tmp2(w * h);
        for(int i = 0; i < h; ++i)
            for(int j = 0; j < w; ++j)
            {
                int ti = w - 1 - j;
                int tj = i;
                tmp2[ti * h + tj] = tmp[i * w + j];
            }
        memcpy(tmp, tmp2.data(), sizeof(uint8_t) * w * h);
        std::swap(w, h);
        break;
    }
    default:
        break;
    }

    // zoom and handle rgba here
    for(int i = 0; i < h; ++i)
    {
        for(int j = 0; j < w; ++j)
        {
            uint8_t p = tmp[i * w + j];
            for(int m = 0; m < rz; ++m)
            {
                for(int n = 0; n < rz; ++n)
                {
                    int di = ((i * rz + m) * rz * w) + (j * rz) + n;
                    if(rgba)
                        palette_rgba(settings.recording_palette, p, &pixels[di * 4]);
                    else
                        pixels[di] = p;
                }
            }
        }
    }

    return pixels.data();
}
