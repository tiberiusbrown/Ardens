#include "imgui.h"

#include "common.hpp"

#include <inttypes.h>
#include <string.h>

static void hotspot_row(int i)
{
    using namespace ImGui;
    auto const& h = settings.profiler_group_symbols ?
        app.emulator.profiler_state.hotspots_symbol[i] :
        app.emulator.profiler_state.hotspots[i];
    uint16_t addr_begin = app.emulator.core_state.cpu.disassembled_prog[h.begin].addr;
    uint16_t addr_end   = app.emulator.core_state.cpu.disassembled_prog[h.end].addr;
    TableSetColumnIndex(0);
    char b[16];
    auto pos = GetCursorPos();
    snprintf(b, sizeof(b), "##row%04x", i);
    if(Selectable(b, app.profiler_selected_hotspot == i,
        ImGuiSelectableFlags_SpanAllColumns))
    {
        if(app.profiler_selected_hotspot == i) app.profiler_selected_hotspot = -1;
        else
        {
            app.disassembly_scroll_addr = (addr_begin + addr_end) / 2;
            app.profiler_selected_hotspot = i;
            settings.open_disassembly = true;
            update_settings();
            SetWindowFocus("Disassembly");
        }
    }
    SetCursorPos(pos);
    if(settings.profiler_cycle_counts)
    {
        Text("%12" PRIu64 "  ", h.count);
        SameLine();
    }
    Text("%6.2f%%", double(h.count) * 100 / app.emulator.profiler_state.cached_total_with_sleep);
    SameLine();
    Text("0x%04x-0x%04x", addr_begin, addr_end);
    
    if(!app.emulator.program_state.elf) return;
    auto const* sym = app.emulator.symbol_for_prog_addr(addr_begin);
    if(!sym) return;
    SameLine();
    TextUnformatted(sym->name.c_str());
}

static void show_hotspots()
{
    using namespace ImGui;

    auto n = app.emulator.profiler_state.num_hotspots;
    if(settings.profiler_group_symbols)
        n = (uint32_t)app.emulator.profiler_state.hotspots_symbol.size();
    if(n <= 0) return;

    ImGuiTableFlags flags = 0;
    flags |= ImGuiTableFlags_ScrollY;
    flags |= ImGuiTableFlags_RowBg;
    flags |= ImGuiTableFlags_SizingFixedFit;

    Separator();
    {
        float active_frac = float(
            double(app.emulator.profiler_state.cached_total) /
            app.emulator.profiler_state.cached_total_with_sleep);
        char buf[32];
        snprintf(buf, sizeof(buf), "CPU Active: %.1f%%", active_frac * 100);
        ProgressBar(active_frac, ImVec2(-FLT_MIN, 0), buf);
    }

    Separator();

    if(BeginTable("##ScrollingRegion", 1, flags))
    {
        ImGuiListClipper clipper;
        clipper.Begin((int)n);
        while(clipper.Step())
        {
            for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                TableNextRow();
                hotspot_row(i);
            }
        }
        EndTable();
    }
}

void window_profiler(bool& open)
{
    using namespace ImGui;
    if(!open) return;
    
    SetNextWindowSize({ 150 * app.pixel_ratio, 300 * app.pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Profiler", &open) && app.emulator.core_state.cpu.decoded)
    {
        if(app.emulator.profiler_state.enabled)
        {
            if(Button("Stop Profiling"))
            {
                app.emulator.profiler_state.enabled = false;
                app.emulator.profiler_state.cached_total = app.emulator.profiler_state.total;
                app.emulator.profiler_state.cached_total_with_sleep = app.emulator.profiler_state.total_with_sleep;
                app.emulator.profiler_build_hotspots();
            }
        }
        else
        {
            if(Button("Start Profiling"))
            {
                app.emulator.profiler_reset();
                app.emulator.profiler_state.enabled = true;
            }
        }
        SameLine();
        if(Checkbox("Cycle Counts", &settings.profiler_cycle_counts))
            update_settings();
        SameLine();
        if(!app.emulator.program_state.elf)
        {
            BeginDisabled();
            settings.profiler_group_symbols = false;
        }
        if(Checkbox("Group by Symbol", &settings.profiler_group_symbols))
            update_settings();
        if(!app.emulator.program_state.elf) EndDisabled();

        show_hotspots();
    }
    End();
}
