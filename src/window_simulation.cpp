#include "imgui.h"

#include "common.hpp"

#include <cmath>

static int slider_val = 12;
static int const SLIDERS[] = {
    10000000, 5000000, 2000000,
    1000000, 500000, 200000,
    100000, 50000, 20000,
    10000, 5000, 2000,
    1000,
    500, 200,
};

static float ttslider = 1.f;

void window_simulation(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 200 * pixel_ratio, 100 * pixel_ratio }, ImGuiCond_FirstUseEver);
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
            load_savedata();
        }
        SameLine();
        if(arduboy.paused)
        {
            if(Button("Continue"))
            {
                arduboy.paused = false;
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

        bool was_paused = arduboy.paused;
        bool was_present = arduboy.is_present_state();
        if(!was_paused)
            BeginDisabled();

        AlignTextToFramePadding();
        TextUnformatted("Forward: ");
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
            arduboy.break_step = arduboy.cpu.stack_frames[arduboy.cpu.num_stack_frames - 1].pc;
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

        AlignTextToFramePadding();
        TextUnformatted("Backward:");
        SameLine();
        if(Button("Step Into###backinto"))
        {
            arduboy.travel_back_single_instr();
            disassembly_scroll_addr = arduboy.cpu.pc * 2;
        }
        if(IsItemHovered())
        {
            BeginTooltip();
            TextUnformatted("Step back one instruction in recorded history, stepping into function calls");
            EndTooltip();
        }
        SameLine();
        if(Button("Step Over###backover"))
        {
            arduboy.travel_back_single_instr_over();
            disassembly_scroll_addr = arduboy.cpu.pc * 2;
        }
        if(IsItemHovered())
        {
            BeginTooltip();
            TextUnformatted("Step back one instruction in recorded history, stepping over function calls");
            EndTooltip();
        }
        SameLine();
        if(Button("Step Out###backout"))
        {
            arduboy.travel_back_single_instr_out();
            disassembly_scroll_addr = arduboy.cpu.pc * 2;
        }
        if(IsItemHovered())
        {
            BeginTooltip();
            TextUnformatted("Step back in recorded history out of the current function to its call site.");
            EndTooltip();
        }

        char buf[64];
        buf[0] = '\0';
        {
            uint32_t dc = uint32_t(arduboy.present_cycle - arduboy.cpu.cycle_count);
            if(arduboy.is_present_state())
                ttslider = 1.f;
            else
            {
                snprintf(buf, sizeof(buf),
                    "-%u cycles, -%.4f ms",
                    dc, double((1.0 / 16e3) * (dc)));
                if(!arduboy.state_history.empty())
                {
                    uint32_t tn = uint32_t(arduboy.cpu.cycle_count - arduboy.state_history[0].cycle);
                    uint32_t td = uint32_t(arduboy.present_cycle - arduboy.state_history[0].cycle);
                    if(td != 0) ttslider = float(tn) / float(td);
                }
            }
        }
        float old_ttslider = ttslider;
        SliderFloat("###ttslider", &ttslider, 0.f, 1.f, buf);
        if(old_ttslider != ttslider && !arduboy.state_history.empty())
        {
            uint64_t cycles = arduboy.present_cycle - arduboy.state_history[0].cycle;
            uint64_t c = uint64_t(std::round(double(ttslider) * cycles));
            c += arduboy.state_history[0].cycle;
            arduboy.travel_to_present();
            arduboy.travel_back_to_cycle(c);
            disassembly_scroll_addr = arduboy.cpu.pc * 2;
        }
        if(was_present)
            BeginDisabled();
        if(Button("Return to Present"))
        {
            ttslider = 1.f;
            arduboy.travel_to_present();
            disassembly_scroll_addr = arduboy.cpu.pc * 2;
        }
        if(IsItemHovered())
        {
            BeginTooltip();
            TextUnformatted("Return to the end of recorded history");
            EndTooltip();
        }
        SameLine();
        if(Button("Set as Present"))
        {
            ttslider = 1.f;
            arduboy.travel_continue();
        }
        if(IsItemHovered())
        {
            BeginTooltip();
            TextUnformatted("Discard recorded history following this point in time");
            EndTooltip();
        }
        if(was_present)
            EndDisabled();

        if(!was_paused)
            EndDisabled();
    }
    End();
}
