#include "common.hpp"

#include "imgui.h"

static bool show_objects = false;
static bool show_labels = false;

static ImGuiTextFilter filter;

void window_symbols(bool& open)
{
    using namespace ImGui;
    if(!open) return;
    SetNextWindowSize({ 400 * pixel_ratio, 400 * pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Symbols", &open) && arduboy->cpu.decoded && arduboy->elf)
    {
        AlignTextToFramePadding();
        TextUnformatted("Show:");
        SameLine();
        Checkbox("Objects", &show_objects);
        SameLine();
        Checkbox("Labels", &show_labels);
        filter.Draw();
        if(IsItemHovered())
        {
            BeginTooltip();
            TextUnformatted(
                "Filter usage:\n"
                "  \"\"         display all lines\n"
                "  \"xxx\"      display lines containing \"xxx\"\n"
                "  \"xxx,yyy\"  display lines containing \"xxx\" or \"yyy\"\n"
                "  \"-xxx\"     hide lines containing \"xxx\"");
            EndTooltip();
        }

        ImGuiTableFlags flags = 0;
        flags |= ImGuiTableFlags_ScrollY;
        flags |= ImGuiTableFlags_RowBg;
        flags |= ImGuiTableFlags_NoClip;
        if(BeginTable("##symbols", 1, flags, { -1, -1 }))
        {
            for(uint16_t addr : arduboy->elf->text_symbols_sorted)
            {
                auto const& sym = arduboy->elf->text_symbols[addr];
                if(!show_objects && sym.object) continue;
                if(!show_labels && sym.notype) continue;
                if(!filter.PassFilter(sym.name.c_str())) continue;
                TableNextRow();
                TableSetColumnIndex(0);
                if(Selectable(sym.name.c_str()))
                    disassembly_scroll_addr = addr, scroll_addr_to_top = true;
            }

            EndTable();
        }
    }
    End();
}
