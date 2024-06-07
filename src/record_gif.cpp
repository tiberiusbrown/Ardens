#include "common.hpp"
#include <cgif.h>

#include <algorithm>

bool gif_recording = false;
uint64_t gif_ps_rem = 0;
static CGIF* gif = nullptr;
static char gif_fname[256];

static inline uint8_t colormap(uint8_t x)
{
    uint8_t r = x;
    if(r == 0)
        r = 1;
    return r;
}

static uint8_t led_modulate(uint8_t x, uint8_t led)
{
    return std::min<uint8_t>(255, x + led);
}

void send_gif_frame(int ds, uint8_t const* pixels)
{
    if(gif_recording && pixels)
    {
        static std::vector<uint8_t> frame;
        int z = recording_filter_zoom();
        z = 128 * 64 * z * z;
        frame.resize(z);

        CGIF_FrameConfig config{};
        config.genFlags =
            CGIF_FRAME_GEN_USE_TRANSPARENCY |
            CGIF_FRAME_GEN_USE_DIFF_WINDOW |
            0;
        config.pImageData = frame.data();
        config.delay = ds;

        static uint8_t palette[256 * 3];
        uint8_t r, g, b;
        arduboy->cpu.led_rgb(r, g, b);
        uint8_t ledm = std::max(std::max(r, g), b);
        if(ledm != 0)
        {
            for(int i = 0; i < 256; ++i)
            {
                uint8_t t[4];
                palette_rgba(settings.recording_palette, uint8_t(i), t);
                palette[3 * i + 0] = t[0];
                palette[3 * i + 1] = t[1];
                palette[3 * i + 2] = t[2];
            }
            config.attrFlags |= CGIF_FRAME_ATTR_USE_LOCAL_TABLE;
            config.pLocalPalette = palette;
            config.numLocalPaletteEntries = 256;
        }

        for(int i = 0; i < z; ++i)
        {
            uint8_t p = pixels[i];
            frame[i] = colormap(p);
        }
        cgif_addframe(gif, &config);
    }
}

void screen_recording_toggle(uint8_t const* pixels)
{
    if(gif_recording)
    {
        send_gif_frame(0, pixels);
        cgif_close(gif);
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
            uint8_t t[4];
            palette_rgba(settings.recording_palette, uint8_t(i), t);
            palette[3 * i + 0] = t[0];
            palette[3 * i + 1] = t[1];
            palette[3 * i + 2] = t[2];
        }
        int depth = 8;
        char const* fname = gif_fname;
#ifdef __EMSCRIPTEN__
        fname = "recording.gif";
#endif
        int z = recording_filter_zoom();
        int w = 128, h = 64;
        if(settings.recording_orientation & 1)
            std::swap(w, h);
        CGIF_Config config{};
        config.width = w * z;
        config.height = h * z;
        config.attrFlags =
            CGIF_ATTR_IS_ANIMATED |
            CGIF_ATTR_HAS_TRANSPARENCY |
            0;
        config.numGlobalPaletteEntries = 256;
        config.numLoops = CGIF_INFINITE_LOOP;
        config.path = fname;
        config.pGlobalPalette = palette;
        gif = cgif_newgif(&config);
        //gif = ge_new_gif(fname, w * z, h * z, palette, depth, -1, 0);
        send_gif_frame(0, pixels);
        gif_ps_rem = 0;
    }
    gif_recording = !gif_recording;
}
