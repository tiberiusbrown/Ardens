#include "imgui.h"

#include "common.hpp"

#include <cmath>
#include <cinttypes>

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

static void sync_secondary_arduboy_to_primary()
{
    if(linked_secondary_arduboy_connected())
    {
        int64_t offset = app.linked_secondary_arduboy_cycle_offset;
        int64_t c = app.emulator->core_state.cpu.cycle_count - offset;
        if(c < 0) c = 0;
        app.linked_secondary_arduboy->travel_to_present();
        app.linked_secondary_arduboy->travel_back_to_cycle((uint64_t)c);
    }
}

static uint64_t tt_min_cycle(absim::arduboy_t const& a)
{
    uint64_t min_cycle = 0;
    if(!a.debugger_state.state_history.empty())
        min_cycle = a.debugger_state.state_history[0].cycle;
    if(!a.debugger_state.input_history.empty())
        min_cycle = std::max(min_cycle, a.debugger_state.input_history[0].cycle);
    return min_cycle;
}

static uint64_t tt_min_cycle()
{
    uint64_t min_cycle = tt_min_cycle(*app.emulator);
    if(linked_secondary_arduboy_connected())
    {
        uint64_t linked_min_cycle = tt_min_cycle(*app.linked_secondary_arduboy);
        linked_min_cycle += app.linked_secondary_arduboy_cycle_offset;
        if((int64_t)linked_min_cycle < 0)
            return min_cycle;
        min_cycle = std::max(min_cycle, linked_min_cycle);
    }
    return min_cycle;
}

void window_simulation(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 200 * app.pixel_ratio, 100 * app.pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Simulation", &open) && app.emulator->core_state.cpu.decoded)
    {
        SliderInt("Speed:", &slider_val, 0, sizeof(SLIDERS) / sizeof(int) - 1, "");
        app.simulation_slowdown = SLIDERS[slider_val];
        SameLine();
        if(app.simulation_slowdown == 1000)
            TextUnformatted("Normal");
        else if(app.simulation_slowdown < 1000)
            Text("%dx faster", 1000 / app.simulation_slowdown);
        else
            Text("%dx slower", app.simulation_slowdown / 1000);
        if(Button("Reset"))
        {
            app.emulator->debugger_state.paused = false;
            reset_primary_simulation();
        }
        SameLine();
        if(app.emulator->debugger_state.paused)
        {
            if(Button("Continue"))
            {
                app.emulator->debugger_state.paused = false;
            }
        }
        else
        {
            if(Button("Pause"))
            {
                app.emulator->debugger_state.paused = true;
                app.emulator->core_state.ps_rem = 0;
                app.disassembly_scroll_addr = app.emulator->core_state.cpu.pc * 2;
            }
        }
        SameLine();
        AlignTextToFramePadding();
        Text("%.3f", (double)app.emulator->core_state.cpu.cycle_count * (1.0 / 16e6));

        bool was_paused = app.emulator->debugger_state.paused;
        bool was_present = app.emulator->is_present_state();
        if(!was_paused)
            BeginDisabled();

        AlignTextToFramePadding();
        TextUnformatted("Forward: ");
        SameLine();
        if(Button("Step Into"))
        {
            advance_primary_instr();
            app.disassembly_scroll_addr = app.emulator->core_state.cpu.pc * 2;
        }
        if(IsItemHovered())
        {
            BeginTooltip();
            TextUnformatted("Execute one instruction, stepping into function calls");
            EndTooltip();
        }
        SameLine();
        if(Button("Step Over") && app.emulator->core_state.cpu.pc < app.emulator->core_state.cpu.decoded_prog.size())
        {
            auto const& i = app.emulator->core_state.cpu.decoded_prog[app.emulator->core_state.cpu.pc];

            if(absim::instr_is_call(i))
            {
                app.emulator->debugger_state.paused = false;

                if(absim::instr_is_two_words(i))
                    app.emulator->debugger_state.break_step = app.emulator->core_state.cpu.pc + 2;
                else
                    app.emulator->debugger_state.break_step = app.emulator->core_state.cpu.pc + 1;
            }
            else
            {
                advance_primary_instr();
                app.disassembly_scroll_addr = app.emulator->core_state.cpu.pc * 2;
            }
        }
        if(IsItemHovered())
        {
            BeginTooltip();
            TextUnformatted("Execute one instruction, stepping over function calls");
            EndTooltip();
        }
        SameLine();
        if(app.emulator->core_state.cpu.num_stack_frames == 0)
            BeginDisabled();
        if(Button("Step Out") && app.emulator->core_state.cpu.num_stack_frames > 0)
        {
            app.emulator->debugger_state.break_step = app.emulator->core_state.cpu.stack_frames[app.emulator->core_state.cpu.num_stack_frames - 1].pc;
            app.emulator->debugger_state.paused = false;
        }
        if(app.emulator->core_state.cpu.num_stack_frames == 0)
            EndDisabled();
        if(IsItemHovered())
        {
            BeginTooltip();
            TextUnformatted("Execute until the current function returns");
            EndTooltip();
        }

        uint64_t min_cycle = tt_min_cycle();
        uint64_t max_cycle = app.emulator->is_present_state() ?
            app.emulator->core_state.cpu.cycle_count :
            app.emulator->debugger_state.present_state.cycle;
        uint64_t cycles = max_cycle - min_cycle;

        AlignTextToFramePadding();
        TextUnformatted("Backward:");
        SameLine();
        if(Button("Step Into###backinto"))
        {
            app.emulator->travel_back_single_instr(min_cycle);
            app.disassembly_scroll_addr = app.emulator->core_state.cpu.pc * 2;
            sync_secondary_arduboy_to_primary();
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
            app.emulator->travel_back_single_instr_over(min_cycle);
            app.disassembly_scroll_addr = app.emulator->core_state.cpu.pc * 2;
            sync_secondary_arduboy_to_primary();
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
            app.emulator->travel_back_single_instr_out(min_cycle);
            app.disassembly_scroll_addr = app.emulator->core_state.cpu.pc * 2;
            sync_secondary_arduboy_to_primary();
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
            uint64_t dc = uint64_t(app.emulator->debugger_state.present_state.cycle - app.emulator->core_state.cpu.cycle_count);
            if(app.emulator->is_present_state())
            {
                ttslider = 1.f;
            }
            else
            {
                snprintf(buf, sizeof(buf),
                    "-%" PRIu64 " cycles, -%.4f ms",
                    dc, double((1.0 / 16e3) * (dc)));
                if(!app.emulator->debugger_state.state_history.empty())
                {
                    uint64_t tn = uint64_t(app.emulator->core_state.cpu.cycle_count - min_cycle);
                    uint64_t td = uint64_t(max_cycle - min_cycle);
                    if((int64_t)tn < 0) ttslider = 0.f;
                    else if(td != 0) ttslider = float(double(tn) / double(td));
                }
            }
        }
        float old_ttslider = ttslider;
        SliderFloat("###ttslider", &ttslider, 0.f, 1.f, buf);
        if(old_ttslider != ttslider && !app.emulator->debugger_state.state_history.empty())
        {
            // Maintain the same cycle offset between linked Arduboys.
            uint64_t c = min_cycle + static_cast<uint64_t>(
                std::round(static_cast<double>(ttslider) * static_cast<double>(cycles)));
            app.emulator->travel_to_present();
            app.emulator->travel_back_to_cycle(c);
            sync_secondary_arduboy_to_primary();
            app.disassembly_scroll_addr = app.emulator->core_state.cpu.pc * 2;
        }
        if(was_present)
            BeginDisabled();
        if(Button("Return to Present"))
        {
            ttslider = 1.f;
            app.emulator->travel_to_present();
            if(linked_secondary_arduboy_connected())
                app.linked_secondary_arduboy->travel_to_present();
            app.disassembly_scroll_addr = app.emulator->core_state.cpu.pc * 2;
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
            app.emulator->travel_continue();
            if(linked_secondary_arduboy_connected())
                app.linked_secondary_arduboy->travel_continue();
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
