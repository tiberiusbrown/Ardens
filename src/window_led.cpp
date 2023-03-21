#include "imgui.h"

#include "common.hpp"

void window_led(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 200, 200 }, ImGuiCond_FirstUseEver);
    if(Begin("LEDs", &open) && arduboy->cpu.decoded)
    {
        uint8_t tx = arduboy->cpu.led_tx();
        uint8_t rx = arduboy->cpu.led_rx();
        uint8_t r, g, b;
        arduboy->cpu.led_rgb(r, g, b);
        auto* draw = GetWindowDrawList();

        auto size = CalcTextSize("   ");

        {
            TextUnformatted("TX:");
            SameLine();
            ImVec2 pmin = GetCursorScreenPos();
            ImVec2 pmax = { pmin.x + size.x, pmin.y + size.y };
            draw->AddRectFilled(pmin, pmax, IM_COL32(tx, tx, tx, 255));
            Dummy(size);
            SameLine();
            TextUnformatted(tx ? "ON " : "OFF");
        }

        SameLine(0.f, size.x);

        {
            TextUnformatted("RX:");
            SameLine();
            ImVec2 pmin = GetCursorScreenPos();
            ImVec2 pmax = { pmin.x + size.x, pmin.y + size.y };
            draw->AddRectFilled(pmin, pmax, IM_COL32(rx, rx, rx, 255));
            Dummy(size);
            SameLine();
            TextUnformatted(rx ? "ON " : "OFF");
        }

        SameLine(0.f, size.x);

        {
            TextUnformatted("RGB:");
            SameLine();
            ImVec2 pmin = GetCursorScreenPos();
            ImVec2 pmax = { pmin.x + size.x, pmin.y + size.y };
            draw->AddRectFilled(pmin, pmax, IM_COL32(r, g, b, 255));
            Dummy(size);
            SameLine();
            Text("%02x %02x %02x", (int)r, (int)g, (int)b);
        }
    }
    End();
}
