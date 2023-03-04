#include "imgui.h"

#include "absim.hpp"
#include "settings.hpp"

#include <inttypes.h>
#include <string.h>

extern absim::arduboy_t arduboy;
extern int profiler_selected_hotspot;
extern int disassembly_scroll_addr;

static void hotspot_row(int i)
{
    using namespace ImGui;
    auto const& h = arduboy.profiler_hotspots[i];
    uint16_t addr_begin = arduboy.cpu.disassembled_prog[h.begin].addr;
    uint16_t addr_end   = arduboy.cpu.disassembled_prog[h.end].addr;
    TableSetColumnIndex(0);
    char b[16];
    auto pos = GetCursorPos();
    snprintf(b, sizeof(b), "##row%04x", i);
    if(Selectable(b, profiler_selected_hotspot == i,
        ImGuiSelectableFlags_SpanAllColumns))
    {
        if(profiler_selected_hotspot == i) profiler_selected_hotspot = -1;
        else
        {
            disassembly_scroll_addr = (addr_begin + addr_end) / 2;
            profiler_selected_hotspot = i;
        }
    }
    SetCursorPos(pos);
    if(settings.profiler_cycle_counts)
    {
        Text("%12" PRIu64 "  ", h.count);
        SameLine();
    }
    Text("%6.2f%%", double(h.count) * 100 / arduboy.profiler_total);
    SameLine();
    Text("0x%04x-0x%04x", addr_begin, addr_end);
    
    if(!arduboy.elf) return;
    auto const* sym = arduboy.symbol_for_prog_addr(addr_begin);
    if(!sym) return;
    SameLine();
    TextUnformatted(sym->name.c_str()); 
}

static void show_hotspots()
{
    using namespace ImGui;

    auto n = arduboy.num_hotspots;
    if(n <= 0) return;

    ImGuiTableFlags flags = 0;
    flags |= ImGuiTableFlags_ScrollY;
    flags |= ImGuiTableFlags_RowBg;
    flags |= ImGuiTableFlags_SizingFixedFit;

    Separator();

    {
        float active_frac = float(
            double(arduboy.profiler_total) /
            arduboy.profiler_total_with_sleep);
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
    
    SetNextWindowSize({ 150, 300 }, ImGuiCond_FirstUseEver);
    if(Begin("Profiler", &open) && arduboy.cpu.decoded)
    {
        if(arduboy.profiler_enabled)
        {
            if(Button("Stop Profiling"))
            {
                arduboy.profiler_enabled = false;
                arduboy.profiler_build_hotspots();
            }
        }
        else
        {
            if(Button("Start Profiling"))
            {
                arduboy.profiler_reset();
                arduboy.profiler_enabled = true;
                arduboy.frame_bytes_total = settings.frame_sync_monochrome ? 1024 : 0;
            }
        }
        SameLine();
        if(Checkbox("Cycle Counts", &settings.profiler_cycle_counts))
            update_settings();

        show_hotspots();
    }
    End();
}
