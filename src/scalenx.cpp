#include "common.hpp"

// used for scale4x first stage
static uint8_t tmpbuf[128 * 64 * 2 * 2];

int display_texture_zoom = -1;

static int filter_zoom(int f)
{
    switch(f)
    {
    case FILTER_NONE:    return 1; break;
    case FILTER_SCALE2X: return 2; break;
    case FILTER_SCALE3X: return 3; break;
    case FILTER_SCALE4X: return 4; break;
    default:             return 1; break;
    }
}

int display_filter_zoom()
{
    return filter_zoom(settings.display_filtering);
}

int recording_filter_zoom()
{
    return
        settings.recording_zoom *
        filter_zoom(settings.recording_filtering);
}

void recreate_display_texture()
{
    int z = display_filter_zoom();
    if(z == display_texture_zoom)
        return;

    if(display_texture)
        SDL_DestroyTexture(display_texture);


    display_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        128 * z,
        64 * z);
}

static void scale2x(uint8_t* dst, uint8_t const* src, bool rgba, int wd, int ht)
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

            if(rgba)
            {
                np = (ni * 2 * wd * 4 * 2) + (nj * 4 * 2);

                dst[np + 0] = dst[np + 1] = dst[np + 2] = e0;
                dst[np + 3] = 255;

                dst[np + 4] = dst[np + 5] = dst[np + 6] = e1;
                dst[np + 7] = 255;

                np += wd * 4 * 2;

                dst[np + 0] = dst[np + 1] = dst[np + 2] = e2;
                dst[np + 3] = 255;

                dst[np + 4] = dst[np + 5] = dst[np + 6] = e3;
                dst[np + 7] = 255;
            }
            else
            {
                np = (ni * 2 * wd * 2) + (nj * 2);
                dst[np + 0] = e0;
                dst[np + 1] = e1;
                dst[np + wd * 2 + 0] = e2;
                dst[np + wd * 2 + 1] = e3;
            }
        }
    }
}

static void scale3x(uint8_t* dst, uint8_t const* src, bool rgba, int wd, int ht)
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

            if(rgba)
            {
                np = (ni * 3 * wd * 4 * 3) + (nj * 4 * 3);

                dst[np + 0] = dst[np + 1] = dst[np + 2] = e0;
                dst[np + 3] = 255;

                dst[np + 4] = dst[np + 5] = dst[np + 6] = e1;
                dst[np + 7] = 255;

                dst[np + 8] = dst[np + 9] = dst[np + 10] = e2;
                dst[np + 11] = 255;

                np += wd * 4 * 3;

                dst[np + 0] = dst[np + 1] = dst[np + 2] = e3;
                dst[np + 3] = 255;

                dst[np + 4] = dst[np + 5] = dst[np + 6] = e4;
                dst[np + 7] = 255;

                dst[np + 8] = dst[np + 9] = dst[np + 10] = e5;
                dst[np + 11] = 255;

                np += wd * 4 * 3;

                dst[np + 0] = dst[np + 1] = dst[np + 2] = e6;
                dst[np + 3] = 255;

                dst[np + 4] = dst[np + 5] = dst[np + 6] = e7;
                dst[np + 7] = 255;

                dst[np + 8] = dst[np + 9] = dst[np + 10] = e8;
                dst[np + 11] = 255;
            }
            else
            {
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
}

static void scalenx_filter(int f, uint8_t* dst, uint8_t const* src, bool rgba)
{
    if(!dst || !src) return;

    switch(f)
    {
    case FILTER_NONE:
        if(rgba)
        {
            for(int i = 0; i < 64; ++i)
            {
                for(int j = 0; j < 128; ++j)
                {
                    auto pi = src[i * 128 + j];
                    *dst++ = pi;
                    *dst++ = pi;
                    *dst++ = pi;
                    *dst++ = 255;
                }
            }
        }
        else
            memcpy(dst, src, 128 * 64);
        break;
    case FILTER_SCALE2X:
        scale2x(dst, src, rgba, 128, 64);
        break;
    case FILTER_SCALE3X:
        scale3x(dst, src, rgba, 128, 64);
        break;
    case FILTER_SCALE4X:
        scale2x(tmpbuf, src, false, 128, 64);
        scale2x(dst, tmpbuf, rgba, 256, 128);
        break;
    default:
        break;
    }

}

void scalenx(uint8_t* dst, uint8_t const* src, bool rgba)
{
    scalenx_filter(settings.display_filtering, dst, src, rgba);
}


uint8_t* recording_pixels(bool rgba)
{
    static std::vector<uint8_t> pixels;
    static uint8_t tmp[128 * 64 * 4 * 4];
    uint8_t const* src = arduboy->display.filtered_pixels.data();

    // someday we'll enable this
    //settings.recording_zoom = 1;

    int z = recording_filter_zoom();
    int w = 128 * z;
    int h = 64 * z;

    pixels.resize(w * h * (rgba ? 4 : 1));

    scalenx_filter(settings.recording_filtering, tmp, src, false);

    // zoom and handle rgba here
    z = filter_zoom(settings.recording_filtering);
    int rz = settings.recording_zoom;
    for(int i = 0; i < z * 64; ++i)
    {
        for(int j = 0; j < z * 128; ++j)
        {
            uint8_t p = tmp[i * z * 128 + j];
            for(int m = 0; m < rz; ++m)
            {
                for(int n = 0; n < rz; ++n)
                {
                    int di = ((i * rz + m) * z * rz * 128) + (j * rz) + n;
                    if(rgba)
                    {
                        pixels[di * 4 + 0] = p;
                        pixels[di * 4 + 1] = p;
                        pixels[di * 4 + 2] = p;
                        pixels[di * 4 + 3] = 255;
                    }
                    else
                        pixels[di] = p;
                }
            }
        }
    }

    return pixels.data();
}
