#include "imgui.h"
#include "imgui_memory_editor.h"

#include "common.hpp"
#include "absim_dwarf.hpp"

#include <fmt/format.h>

static MemoryEditor memed_data_space;

static void globals_usage()
{
    using namespace ImGui;

    if(!arduboy.elf || arduboy.elf->data_symbols_sorted_size.empty())
        return;
    if(!BeginTooltip())
        return;

    constexpr ImGuiTableFlags tf =
        ImGuiTableFlags_NoSavedSettings |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        0;
    if(BeginTable("##globals_usage", 3, tf, { -1, -1 }))
    {
        constexpr ImGuiTableColumnFlags cf =
            ImGuiTableColumnFlags_WidthFixed |
            0;
        TableSetupColumn("Address", cf);
        TableSetupColumn("Bytes", cf);
        TableSetupColumn("Name", cf);
        TableHeadersRow();
        for(auto i : arduboy.elf->data_symbols_sorted_size)
        {
            auto const& sym = arduboy.elf->data_symbols[i];
            TableNextRow();
            TableSetColumnIndex(0);
            Text("0x%04x", sym.addr);
            TableSetColumnIndex(1);
            Text("%4d", sym.size);
            TableSetColumnIndex(2);
            TextUnformatted(sym.name.c_str());
        }
        EndTable();
    }

    EndTooltip();
}

#ifdef ARDENS_LLVM
static std::string dwarf_value_string_addr(
    llvm::DWARFDie die, uint32_t addr, bool prog,
    uint32_t bit_offset, uint32_t bit_size,
    absim::dwarf_value_base base = absim::dwarf_value_base::dec)
{
    absim::dwarf_span mem;
    if(prog) mem = absim::to_dwarf_span(arduboy.cpu.prog);
    else     mem = absim::to_dwarf_span(arduboy.cpu.data);
    return absim::dwarf_value_string(die, mem.offset(addr), bit_offset, bit_size, base);
}
#endif

static bool dwarf_symbol_tooltip(uint16_t addr, absim::elf_data_symbol_t const& sym, bool prog)
{
#ifdef ARDENS_LLVM

    if(!(addr >= sym.addr && addr < sym.addr + sym.size))
        return false;
    uint16_t offset = uint16_t(addr - sym.addr);

    auto* dwarf = arduboy.elf->dwarf_ctx.get();
    if(!dwarf) return false;

    // just do a full search for global
    char const* name = nullptr;
    llvm::DWARFDie type;
    for(auto const& kv : arduboy.elf->globals)
    {
        auto const& g = kv.second;
        if(prog != g.text) continue;
        auto* cu = dwarf->getCompileUnitForOffset(g.cu_offset);
        if(!cu) continue;
        type = cu->getDIEForOffset(g.type);
        if(!(addr >= g.addr && addr < g.addr + absim::dwarf_size(type)))
            continue;
        name = kv.first.c_str();
        break;
    }
    if(!name) return false;

    absim::dwarf_primitive_t prim;
    prim.bit_offset = 0;
    prim.bit_size = 0;
    prim.expr = name;
    if(absim::dwarf_find_primitive(type, offset, prim))
    {
        ImGui::Text("0x%04x: %s", addr, prim.expr.c_str());
        if(absim::dwarf_size(prim.die) > 1)
        {
            ImGui::SameLine(0.f, 0.f);
            ImGui::Text(" [offset %d]", (int)prim.offset);
        }
        ImGui::Separator();
        auto type_str = absim::dwarf_type_string(prim.die);
        if(prim.bit_size != 0)
            type_str += fmt::format(" : {}", prim.bit_size);
        ImGui::Text("Type:   %s", type_str.c_str());
        ImGui::Text("Value:  %s", dwarf_value_string_addr(
            prim.die, addr - prim.offset, prog, prim.bit_offset, prim.bit_size).c_str());
        auto size = absim::dwarf_size(prim.die);
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
                if(prog && a + j < arduboy.cpu.prog.size() ||
                    !prog && a + j < arduboy.cpu.data.size())
                {
                    ImGui::Text("%02x", prog ?
                        arduboy.cpu.prog[a + j] :
                        arduboy.cpu.data[a + j]);
                }
                else
                    ImGui::TextUnformatted("??");
                if(size > 1 && j == prim.offset)
                    ImGui::PopStyleColor();
                ImGui::SameLine(0.f, 0.f);
            }
        }
    }
    else if(absim::dwarf_size(type) <= 1)
        ImGui::Text("0x%04x: %s", addr, sym.name.c_str());
    else
        ImGui::Text("0x%04x: %s [offset %d]", addr, sym.name.c_str(), (int)offset);
    return true;

#else
    return false;
#endif
}

void symbol_tooltip(uint16_t addr, absim::elf_data_symbol_t const& sym, bool prog)
{
    using namespace ImGui;

    if(dwarf_symbol_tooltip(addr, sym, prog))
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
    if(addr < arduboy.cpu.data.size())
    {
        rd = arduboy.breakpoints_rd.test(addr);
        wr = arduboy.breakpoints_wr.test(addr);
    }
    BeginDisabled(addr < 32 || addr >= arduboy.cpu.data.size());
    AlignTextToFramePadding();
    TextUnformatted("Break on:");
    SameLine();
    Checkbox("Read", &rd);
    SameLine();
    Checkbox("Write", &wr);
    EndDisabled();
    if(addr < arduboy.cpu.data.size())
    {
        arduboy.breakpoints_rd[addr] = rd;
        arduboy.breakpoints_wr[addr] = wr;
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
    else if(off >= arduboy.cpu.min_stack)
    {
        color = IM_COL32(70, 0, 90, 255);
        r = true;
    }
    else if(arduboy.cpu.stack_check > 0x100 && off >= arduboy.cpu.stack_check)
    {
        color = IM_COL32(45, 45, 45, 255);
        r = true;
    }
    else if(auto const* sym = arduboy.symbol_for_data_addr((uint16_t)off))
    {
        color = darker_color_for_index(sym->color_index);
        r = true;
    }
    if(off < arduboy.cpu.data.size() && (
        arduboy.breakpoints_rd.test(off) ||
        arduboy.breakpoints_wr.test(off)))
    {
        color = IM_COL32(150, 50, 50, 255);
        r = true;
    }
    return r;
}

void hover_data_space(uint16_t addr)
{
    using namespace ImGui;
    auto const* sym = arduboy.symbol_for_data_addr(addr);
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
                            if(arduboy.cpu.data[addr] & mask)
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
        SetNextWindowSize({ 200 * pixel_ratio, 400 * pixel_ratio }, ImGuiCond_FirstUseEver);
        if(Begin("CPU Data Space", &open) && arduboy.cpu.decoded)
        {
            if(arduboy.cpu.stack_check > 0x100)
            {
                float df = float(arduboy.cpu.stack_check - 0x100) / 2560;
                Text("Globals:     %d bytes (%d%%)",
                    (int)(arduboy.cpu.stack_check - 0x100),
                    (int)std::round(df * 100));
                if(arduboy.elf && !arduboy.elf->data_symbols_sorted_size.empty())
                {
                    SameLine();
                    PushStyleColor(ImGuiCol_Text, IM_COL32(150, 250, 150, 255));
                    TextUnformatted("[hover for space usage]");
                    PopStyleColor();
                    if(IsItemHovered())
                        globals_usage();
                }
                Text("Stack:       %d/%d bytes used (%d free)",
                    (int)(2560 + 256 - arduboy.cpu.sp()),
                    (int)(2560 + 256 - arduboy.cpu.stack_check),
                    (int)(arduboy.cpu.sp() - arduboy.cpu.stack_check));
                Text("Stack (max): %d/%d bytes used (%d free)",
                    (int)(2560 + 256 - arduboy.cpu.min_stack),
                    (int)(2560 + 256 - arduboy.cpu.stack_check),
                    (int)(arduboy.cpu.min_stack - arduboy.cpu.stack_check));
                Separator();
            }

            memed_data_space.DrawContents(
                arduboy.cpu.data.data(),
                arduboy.cpu.data.size());

            auto addr = memed_data_space.DataPreviewAddr;
            draw_memory_breakpoints(addr);
        }
        End();
    }
}
