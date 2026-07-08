#include "common.hpp"

#include "imgui.h"

#include <algorithm>

static char addr_buf[5];

static int hex_value(char c)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void update_display_buffer_texture()
{
    if(app.display_buffer_addr < 0) return;
    if(app.display_buffer_addr >= (int)app.emulator->core_state.cpu.data.size()) return;

    static uint8_t pixels[128 * 64 * 4];
    uint8_t* bpixels = (uint8_t*)pixels;

    app.display_buffer_w = std::min(app.display_buffer_w, 128);
    app.display_buffer_h = std::min(app.display_buffer_h, 64);
    for(int i = 0; i < app.display_buffer_h; ++i)
    {
        for(int j = 0; j < app.display_buffer_w; ++j)
        {
            size_t n = (size_t)app.display_buffer_addr + (i / 8) * app.display_buffer_w + j;
            uint8_t pi = 128;
            if(n < app.emulator->core_state.cpu.data.size())
            {
                uint8_t d = app.emulator->core_state.cpu.data[n];
                pi = (d & (1 << (i % 8))) ? 255 : 0;
            }
            *bpixels++ = pi;
            *bpixels++ = pi;
            *bpixels++ = pi;
            *bpixels++ = 255;
        }
    }

    platform_update_texture(app.display_buffer_texture, pixels, sizeof(pixels));
    platform_texture_scale_nearest(app.display_buffer_texture);
}

void window_display_buffer(bool& open)
{
    using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 400 * app.pixel_ratio, 400 * app.pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Display Buffer (RAM)", &open) && app.emulator->core_state.cpu.decoded)
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
            if(a >= 0 && a < (int)app.emulator->core_state.cpu.prog.size())
                app.display_buffer_addr = a;
        }
        if(app.emulator->program_state.elf)
        {
            if(app.display_buffer_addr < 0)
            {
                for(auto const& kv : app.emulator->program_state.elf->data_symbols)
                {
                    if(kv.second.name != "Arduboy2Base::sBuffer") continue;
                    app.display_buffer_addr = kv.first;
                    snprintf(addr_buf, sizeof(addr_buf), "%x", (unsigned)app.display_buffer_addr);
                }
            }
            SameLine();
            SetNextItemWidth(GetContentRegionAvail().x);
            if(BeginCombo("##symbol", "Symbol...", ImGuiComboFlags_HeightLarge))
            {
                for(uint16_t addr : app.emulator->program_state.elf->data_symbols_sorted)
                {
                    auto const& sym = app.emulator->program_state.elf->data_symbols[addr];
                    if(sym.weak || sym.notype) continue;
                    if(Selectable(sym.name.c_str()))
                    {
                        app.display_buffer_addr = addr;
                        snprintf(addr_buf, sizeof(addr_buf), "%x", (unsigned)addr);
                    }
                }
                EndCombo();
            }
        }
        SliderInt("Width", &app.display_buffer_w, 1, 128);
        SliderInt("Height", &app.display_buffer_h, 1, 64);

        update_display_buffer_texture();

        {
            auto t = GetContentRegionAvail();
            float w = (float)app.display_buffer_w;
            float h = (float)app.display_buffer_h;
            while(w + app.display_buffer_w < t.x && h + app.display_buffer_h < t.y)
                w += app.display_buffer_w, h += app.display_buffer_h;
            float u = (float)app.display_buffer_w / 128;
            float v = (float)app.display_buffer_h / 64;
            Image(app.display_buffer_texture, { w, h }, { 0, 0 }, { u, v });
        }
    }
    End();
}
