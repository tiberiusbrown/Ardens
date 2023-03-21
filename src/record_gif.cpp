#include "common.hpp"

#include "gifenc.h"

bool gif_recording = false;
uint32_t gif_ms_rem = 0;
static ge_GIF* gif = nullptr;
static char gif_fname[256];

static inline uint8_t colormap(uint8_t x)
{
    uint8_t r = x;
    if(r == 255)
        r = 254;
    return r;
}

void send_gif_frame(int ds, uint8_t const* pixels)
{
    if(gif_recording && pixels)
    {
        int z = recording_filter_zoom();
        z = 128 * 64 * z * z;
        for(int i = 0; i < z; ++i)
        {
            uint8_t p = pixels[i];
            gif->frame[i] = colormap(p);
        }
        ge_add_frame(gif, ds);
    }
}

void screen_recording_toggle(uint8_t const* pixels)
{
    if(gif_recording)
    {
        send_gif_frame(0, pixels);
        ge_close_gif(gif);
#ifdef __EMSCRIPTEN__
        file_download("recording.gif", gif_fname, "image/gif");
#endif
    }
    else
    {
        time_t rawtime;
        struct tm* ti;
        time(&rawtime);
        ti = localtime(&rawtime);
        (void)snprintf(gif_fname, sizeof(gif_fname),
            "recording_%04d%02d%02d%02d%02d%02d.gif",
            ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
            ti->tm_hour + 1, ti->tm_min, ti->tm_sec);
        uint8_t palette[256 * 3];
        for(int i = 0; i < 256; ++i)
        {
            palette[3 * i + 0] = i;
            palette[3 * i + 1] = i;
            palette[3 * i + 2] = i;
        }
        int depth = 8;
        char const* fname = gif_fname;
#ifdef __EMSCRIPTEN__
        fname = "recording.gif";
#endif
        int z = recording_filter_zoom();
        gif = ge_new_gif(fname, 128 * z, 64 * z, palette, depth, -1, 0);
        send_gif_frame(0, pixels);
        gif_ms_rem = 0;
    }
    gif_recording = !gif_recording;
}
