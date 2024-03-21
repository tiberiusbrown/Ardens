#include "imgui.h"
#include "common.hpp"

void window_timers(bool& open)
{
    using namespace ImGui;

    if(!open || !arduboy) return;

    SetNextWindowSize({ 400 * pixel_ratio, 200 * pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Timers", &open) && arduboy->cpu.decoded)
    {
        auto const& t0 = arduboy->cpu.timer0;
        auto const& t1 = arduboy->cpu.timer1;
        auto const& t3 = arduboy->cpu.timer3;
        auto const& t4 = arduboy->cpu.timer4;

        if(CollapsingHeader("Timer 0"))
        {
            Text("Divider: %u", t0.divider);
            Text("TCNT0:   %u", t0.tcnt);
            Text("TOP:     %u", t0.top);
            Text("TOV:     %u", t0.tov);
            Text("OCR0A:   %u", t0.ocrNa);
            Text("OCR0B:   %u", t0.ocrNb);
        }
    }
    End();
}
