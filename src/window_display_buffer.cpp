#include "common.hpp"

#include "imgui.h"

#include <SDL.h>

#include <algorithm>

static char addr_buf[5];

static int hex_value(char c)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void platform_update_texture()
{
    if(display_buffer_addr < 0) return;
    if(display_buffer_addr >= arduboy->cpu.data.size()) return;
    void* pixels = nullptr;
    int pitch = 0;
    SDL_LockTexture(display_buffer_texture, nullptr, &pixels, &pitch);
    uint8_t* bpixels = (uint8_t*)pixels;
    display_buffer_w = std::min(display_buffer_w, 128);
    display_buffer_h = std::min(display_buffer_h, 64);
    for(int i = 0; i < display_buffer_h; ++i)
    {
        for(int j = 0; j < display_buffer_w; ++j)
        {
            size_t n = (size_t)display_buffer_addr + (i / 8) * display_buffer_w + j;
            uint8_t pi = 128;
            if(n < arduboy->cpu.data.size())
            {
                uint8_t d = arduboy->cpu.data[n];
                pi = (d & (1 << (i % 8))) ? 255 : 0;
            }
            *bpixels++ = pi;
            *bpixels++ = pi;
            *bpixels++ = pi;
            *bpixels++ = 255;
        }
    }
    SDL_UnlockTexture(display_buffer_texture);
}

void window_display_buffer(bool& open)
{
    using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 400, 400 }, ImGuiCond_FirstUseEver);
    if(Begin("Display Buffer (RAM)", &open) && arduboy->cpu.decoded)
    {
        AlignTextToFramePadding();
        TextUnformatted("Address: 0x");
        SameLine(0.f, 0.f);
        SetNextItemWidth(
            CalcTextSize("FFFF").x +
            GetStyle().FramePadding.x * 2);
        if(InputText(
            "##addr",
            addr_buf, sizeof(addr_buf),
            ImGuiInputTextFlags_CharsHexadecimal |
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll))
        {
            int a = 0;
            char* b = addr_buf;
            while(*b != 0)
                a = (a << 4) + hex_value(*b++);
            if(a >= 0 && a < arduboy->cpu.prog.size())
                display_buffer_addr = a;
        }
        if(arduboy->elf)
        {
            SameLine();
            SetNextItemWidth(GetContentRegionAvail().x);
            if(BeginCombo("##symbol", "Symbol...", ImGuiComboFlags_HeightLarge))
            {
                for(uint16_t addr : arduboy->elf->data_symbols_sorted)
                {
                    auto const& sym = arduboy->elf->data_symbols[addr];
                    if(sym.weak || sym.notype) continue;
                    if(Selectable(sym.name.c_str()))
                    {
                        display_buffer_addr = addr;
                        snprintf(addr_buf, sizeof(addr_buf), "%x", (unsigned)addr);
                    }
                }
                EndCombo();
            }
        }
        SliderInt("Width", &display_buffer_w, 1, 128);
        SliderInt("Height", &display_buffer_h, 1, 64);

        platform_update_texture();

        {
            auto t = GetContentRegionAvail();
            float w = display_buffer_w, h = display_buffer_h;
            while(w + display_buffer_w < t.x && h + display_buffer_h < t.y)
                w += display_buffer_w, h += display_buffer_h;
            float u = (float)display_buffer_w / 128;
            float v = (float)display_buffer_h / 64;
            Image(display_buffer_texture, { w, h }, { 0, 0 }, { u, v });
        }
    }
    End();
}
