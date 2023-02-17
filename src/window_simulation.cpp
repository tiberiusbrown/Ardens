#include "imgui.h"

#include "absim.hpp"

extern absim::arduboy_t arduboy;
extern int disassembly_scroll_addr;
extern int simulation_slowdown;

static int slider_val = 9;
static int const SLIDERS[] = {
    1000000, 500000, 200000,
    100000, 50000, 20000,
    10000, 5000, 2000,
    1000,
    500, 200,
};

void window_simulation(bool& open)
{
	using namespace ImGui;
    if(!open) return;
    
    SetNextWindowSize({ 200, 100 }, ImGuiCond_FirstUseEver);
    if(Begin("Simulation", &open) && arduboy.cpu.decoded)
    {
        SliderInt("Speed:", &slider_val, 0, sizeof(SLIDERS) / sizeof(int) - 1, "");
        simulation_slowdown = SLIDERS[slider_val];
        SameLine();
        if(simulation_slowdown == 1000)
            TextUnformatted("Normal");
        else if(simulation_slowdown < 1000)
            Text("%dx faster", 1000 / simulation_slowdown);
        else
            Text("%dx slower", simulation_slowdown / 1000);
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
            if(Button("Step Into"))
            {
                arduboy.advance_instr();
                disassembly_scroll_addr = arduboy.cpu.pc * 2;
            }
            if(IsItemHovered())
            {
                BeginTooltip();
                TextUnformatted("Execute one instruction, stepping into function calls");
                EndTooltip();
            }
            SameLine();
            if(Button("Step Over") && arduboy.cpu.pc < arduboy.cpu.decoded_prog.size())
            {
                auto const& i = arduboy.cpu.decoded_prog[arduboy.cpu.pc];

                if(absim::instr_is_call(i))
                {
                    arduboy.paused = false;

                    if(absim::instr_is_two_words(i))
                        arduboy.break_step = arduboy.cpu.pc + 2;
                    else
                        arduboy.break_step = arduboy.cpu.pc + 1;
                }
                else
                {
                    arduboy.advance_instr();
                    disassembly_scroll_addr = arduboy.cpu.pc * 2;
                }
            }
            if(IsItemHovered())
            {
                BeginTooltip();
                TextUnformatted("Execute one instruction, stepping over function calls");
                EndTooltip();
            }
            SameLine();
            if(arduboy.cpu.num_stack_frames == 0)
                BeginDisabled();
            if(Button("Step Out") && arduboy.cpu.num_stack_frames > 0)
            {
                arduboy.break_step = arduboy.cpu.stack_frames[arduboy.cpu.num_stack_frames - 1];
                arduboy.paused = false;
            }
            if(arduboy.cpu.num_stack_frames == 0)
                EndDisabled();
            if(IsItemHovered())
            {
                BeginTooltip();
                TextUnformatted("Execute until the current function returns");
                EndTooltip();
            }
        }
        else
        {
            if(Button("Pause"))
            {
                arduboy.paused = true;
                arduboy.ps_rem = 0;
                disassembly_scroll_addr = arduboy.cpu.pc * 2;
            }
        }
    }
    End();
}
