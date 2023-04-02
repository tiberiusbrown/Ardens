#include "imgui.h"
#include "imgui_memory_editor.h"

#include "common.hpp"
#include "dwarf.hpp"

#include <fmt/format.h>

#ifdef ABSIM_LLVM

#ifdef _MSC_VER
#pragma warning(push, 1)
#pragma warning(disable: 4624)
#endif

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFCompileUnit.h>
#include <llvm/DebugInfo/DWARF/DWARFExpression.h>

#ifdef _MSC_VER
#pragma warning(pop) 
#endif

#endif

static MemoryEditor memed_data_space;

static int recurse_type(std::string& expr, uint16_t offset, llvm::DWARFDie die)
{
    if(!die.isValid()) return -1;
    switch(die.getTag())
    {
    case llvm::dwarf::DW_TAG_const_type:
    case llvm::dwarf::DW_TAG_volatile_type:
        return recurse_type(expr, offset, dwarf_type(die));
    case llvm::dwarf::DW_TAG_structure_type:
        for(auto const& p : dwarf_members(die))
        {
            auto size = dwarf_size(p.die);
            if(!(offset >= p.offset && offset < p.offset + size)) continue;
            expr += fmt::format(".{}", dwarf_name(p.die));
            return recurse_type(expr, offset - p.offset, dwarf_type(p.die));
        }
        return offset;
    case llvm::dwarf::DW_TAG_array_type:
    {
        auto type = dwarf_type(die);
        auto size = dwarf_size(type);
        if(size <= 0) break;
        size_t i = offset / size;
        offset %= size;
        expr += fmt::format("[{}]", i);
        return recurse_type(expr, offset, type);
    }
    default:
        if(dwarf_size(die) == 1) return -1;
        break;
    }
    return offset;
}

static bool dwarf_symbol_tooltip(uint16_t addr, absim::elf_data_symbol_t const& sym)
{
#ifdef ABSIM_LLVM

    if(!(addr >= sym.addr && addr < sym.addr + sym.size))
        return false;
    uint16_t offset = uint16_t(addr - sym.addr);

    auto* dwarf = arduboy->elf->dwarf_ctx.get();
    if(!dwarf) return false;

    // just do a full search for global
    char const* name = nullptr;
    llvm::DWARFDie type;
    for(auto const& kv : arduboy->elf->globals)
    {
        auto const& g = kv.second;
        if(g.text) continue;
        auto* cu = dwarf->getCompileUnitForOffset(g.cu_offset);
        if(!cu) continue;
        type = cu->getDIEForOffset(g.type);
        if(!(addr >= g.addr && addr < g.addr + dwarf_size(type)))
            continue;
        name = kv.first.c_str();
        break;
    }
    if(!name) return false;

    dwarf_primitive_t prim;
    prim.expr = name;
    if(dwarf_find_primitive(type, offset, prim))
    {
        ImGui::Text("0x%04x: %s", addr, prim.expr.c_str());
        if(dwarf_size(prim.die) > 1)
        {
            ImGui::SameLine(0.f, 0.f);
            ImGui::Text(" [offset %d]", (int)prim.offset);
        }
        ImGui::Separator();
        ImGui::Text("Type:   %s", dwarf_type_string(prim.die).c_str());
        ImGui::Text("Value:  %s", dwarf_value_string(prim.die, addr - prim.offset, false).c_str());
        auto size = dwarf_size(prim.die);
        if(size <= 4)
        {
            ImGui::Text("Raw:    0x");
            ImGui::SameLine(0.f, 0.f);
            uint32_t a = addr - prim.offset;
            for(uint32_t i = 0; i < size; ++i)
            {
                auto j = size - i - 1;
                if(size > 1 && j == prim.offset)
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 255, 255));
                ImGui::Text("%02x", arduboy->cpu.data[a + j]);
                if(size > 1 && j == prim.offset)
                    ImGui::PopStyleColor();
                ImGui::SameLine(0.f, 0.f);
            }
        }
    }
    else if(dwarf_size(type) <= 1)
        ImGui::Text("0x%04x: %s", addr, sym.name.c_str());
    else
        ImGui::Text("0x%04x: %s [offset %d]", addr, sym.name.c_str(), (int)offset);
    return true;

#else
    return false;
#endif
}

static void symbol_tooltip(uint16_t addr, absim::elf_data_symbol_t const& sym)
{
    using namespace ImGui;

    if(dwarf_symbol_tooltip(addr, sym))
        return;
    
    if(sym.size > 1)
        Text("0x%04x: %s [byte %d]", addr, sym.name.c_str(), int(addr - sym.addr));
    else
        Text("0x%04x: %s", addr, sym.name.c_str());
}

static void draw_memory_breakpoints(size_t addr)
{
    using namespace ImGui;
    bool rd = false;
    bool wr = false;
    if(addr < arduboy->cpu.data.size())
    {
        rd = arduboy->breakpoints_rd.test(addr);
        wr = arduboy->breakpoints_wr.test(addr);
    }
    BeginDisabled(addr < 32 || addr >= arduboy->cpu.data.size());
    AlignTextToFramePadding();
    TextUnformatted("Break on:");
    SameLine();
    Checkbox("Read", &rd);
    SameLine();
    Checkbox("Write", &wr);
    EndDisabled();
    if(addr < arduboy->cpu.data.size())
    {
        arduboy->breakpoints_rd[addr] = rd;
        arduboy->breakpoints_wr[addr] = wr;
    }
}

// dark2
static ImU32 const SYMBOL_COLORS[] =
{
    IM_COL32( 27, 158, 119, 255),
    IM_COL32(217,  95,   2, 255),
    IM_COL32(117, 112, 179, 255),
    IM_COL32(231,  41, 138, 255),
    IM_COL32(102,  66,  30, 255),
    IM_COL32(230, 171,   2, 255),
    IM_COL32(166, 118,  29, 255),
    IM_COL32(102, 102, 102, 255),
};
constexpr auto NUM_COLORS = sizeof(SYMBOL_COLORS) / sizeof(ImU32);

uint32_t color_for_index(size_t index)
{
    return SYMBOL_COLORS[index % NUM_COLORS];
}

uint32_t darker_color_for_index(size_t index)
{
    uint32_t c = color_for_index(index);
    c >>= 1;
    c &= 0x7f7f7f7f;
    c |= (uint32_t(0xff) << IM_COL32_A_SHIFT);
    return c;
}

static bool highlight_func(ImU8 const* data, size_t off, ImU32& color)
{
    bool r = false;
    if(off < 0x20)
    {
        color = IM_COL32(20, 20, 50, 255);
        r = true;
    }
    else if(off < 0x100)
    {
        color = absim::REG_INFO[off].name ?
            IM_COL32(50, 50, 20, 255) :
            IM_COL32(0, 0, 0, 255);
        r = true;
    }
    else if(off >= arduboy->cpu.min_stack)
    {
        color = IM_COL32(70, 0, 90, 255);
        r = true;
    }
    else if(arduboy->cpu.stack_check > 0x100 && off >= arduboy->cpu.stack_check)
    {
        color = IM_COL32(45, 45, 45, 255);
        r = true;
    }
    else if(auto const* sym = arduboy->symbol_for_data_addr((uint16_t)off))
    {
        color = darker_color_for_index(sym->color_index);
        r = true;
    }
    if(off < arduboy->cpu.data.size() && (
        arduboy->breakpoints_rd.test(off) ||
        arduboy->breakpoints_wr.test(off)))
    {
        color = IM_COL32(150, 50, 50, 255);
        r = true;
    }
    return r;
}

void hover_data_space(uint16_t addr)
{
    using namespace ImGui;
    auto const* sym = arduboy->symbol_for_data_addr(addr);
    BeginTooltip();
    if(addr < 256)
    {
        auto const& r = absim::REG_INFO[addr];
        if(r.name)
        {
            constexpr auto UNUSED = IM_COL32(50, 50, 50, 255);
            constexpr auto BIT_SET = IM_COL32(30, 30, 100, 255);
            Text("0x%02x: %s", addr, r.name);
            if(addr >= 32)
            {
                Separator();
                Dummy({ 1.f, 4.f });
                if(BeginTable("##bits", 8,
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_SizingStretchSame))
                {
                    TableNextRow();
                    for(int i = 0; i < 8; ++i)
                    {
                        TableSetColumnIndex(i);
                        char const* bit = r.bits[7 - i];
                        if(bit)
                        {
                            uint8_t mask = 0x80 >> i;
                            if(arduboy->cpu.data[addr] & mask)
                                TableSetBgColor(ImGuiTableBgTarget_CellBg, BIT_SET);
                            TextUnformatted(bit);
                        }
                        else
                            TableSetBgColor(ImGuiTableBgTarget_CellBg, UNUSED);
                    }
                    EndTable();
                }
            }
        }
        else
            Text("0x%02x: Reserved", addr);
    }
    else if(sym)
    {
        symbol_tooltip(addr, *sym);
    }
    else
    {
        Text("0x%04x", addr);
    }
    EndTooltip();
}

static void hover_func(ImU8 const* data, size_t off)
{
    (void)data;
    hover_data_space((uint16_t)off);
}

void window_data_space(bool& open)
{
    using namespace ImGui;

    {
        static bool first = true;
        if(first)
        {
            memed_data_space.OptShowDataPreview = true;
            memed_data_space.PreviewDataType = ImGuiDataType_U8;
            memed_data_space.OptFooterExtraHeight = GetFrameHeightWithSpacing();
            memed_data_space.HighlightFn = highlight_func;
            memed_data_space.HoverFn = hover_func;
            first = false;
        }
    }

    if(open)
    {
        SetNextWindowSize({ 200, 400 }, ImGuiCond_FirstUseEver);
        if(Begin("CPU Data Space", &open) && arduboy->cpu.decoded)
        {
            memed_data_space.DrawContents(
                arduboy->cpu.data.data(),
                arduboy->cpu.data.size());

            auto addr = memed_data_space.DataPreviewAddr;
            draw_memory_breakpoints(addr);
        }
        End();
    }
}
