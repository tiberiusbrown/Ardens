#include "imgui.h"

#include "absim.hpp"

extern absim::arduboy_t arduboy;
extern int disassembly_scroll_addr;
extern int simulation_slowdown;

static int slider_val;
static int const SLIDERS[] = { 1, 10, 100, 1000, 10000, 100000, 1000000 };

void window_simulation(bool& open)
{
	using namespace ImGui;
    if(!open) return;
    
    SetNextWindowSize({ 200, 100 }, ImGuiCond_FirstUseEver);
    if(Begin("Simulation", &open) && arduboy.cpu.decoded)
    {
        SliderInt("Slowdown:", &slider_val, 0, sizeof(SLIDERS) / sizeof(int) - 1, "");
        simulation_slowdown = SLIDERS[slider_val];
        SameLine();
        Text("%dx", simulation_slowdown);
        if(Button("Reset"))
        {
            arduboy.paused = false;
            arduboy.reset();
        }
        SameLine();
        if(arduboy.paused)
        {
            if(Button("Continue"))
                arduboy.paused = false;
            SameLine();
            if(Button("Step"))
            {
                arduboy.advance_instr();
                disassembly_scroll_addr = arduboy.cpu.pc * 2;
            }
        }
        else
        {
            if(Button("Pause"))
            {
                arduboy.paused = true;
                while(arduboy.cpu.cycles_till_next_instr != 0)
                    arduboy.cpu.advance_cycle();
                disassembly_scroll_addr = arduboy.cpu.pc * 2;
            }
        }
    }
    End();
}
