#include "common.hpp"

#include "imgui.h"

#ifdef ARDENS_LLVM

#include "dwarf.hpp"

#include <fmt/format.h>

static bool show_io = false;

constexpr int MAX_VALUE_CHARS = 64;

static std::vector<std::string> watches;

static uint32_t array_size(llvm::DWARFDie& die)
{
    if(!die.hasChildren()) return 0;
    if(die.getTag() != llvm::dwarf::DW_TAG_array_type) return 0;
    uint32_t n = 1;
    for(auto child : die.children())
    {
        if(child.getTag() != llvm::dwarf::DW_TAG_subrange_type)
            return 0;
        if(auto tu = child.find(llvm::dwarf::DW_AT_upper_bound))
            if(auto u = tu->getAsUnsignedConstant())
                n *= ((uint32_t)u.value() + 1);
    }
    return n;
}

static llvm::DWARFDie remove_cv_typedefs_die(llvm::DWARFDie die)
{
    for(;;)
    {
        auto tag = die.getTag();
        if(tag == llvm::dwarf::DW_TAG_const_type ||
            tag == llvm::dwarf::DW_TAG_volatile_type ||
            tag == llvm::dwarf::DW_TAG_typedef)
        {
            die = dwarf_type(die);
        }
        else
            break;
    }
    return die;
}

static bool do_var_row(
    int id,
    char const* name, bool root, bool local,
    dwarf_span mem, bool text, llvm::DWARFDie die,
    uint32_t bit_offset = 0, uint32_t bit_size = 0)
{
    using namespace ImGui;
    std::string t;
    bool tree = false;
    enum { T_NONE, T_STRUCT, T_ARRAY } tree_type = T_NONE;
    uint32_t n = 0;
    bool remove = false;

    auto resolved_die = remove_cv_typedefs_die(die);

    TableNextRow();

    PushID(id);

    if(TableSetColumnIndex(0))
    {
        float indent =
            GetStyle().FramePadding.x * 2.f +
            GetStyle().ItemSpacing.x +
            CalcTextSize("-").x;
        if(!local && root)
        {
            if(SmallButton("-"))
                remove = true;
            indent = 0.f;
            SameLine();
        }

        if(indent != 0.f) Indent(indent);
        auto tag = resolved_die.getTag();
        if(resolved_die.hasChildren() && (
            tag == llvm::dwarf::DW_TAG_structure_type ||
            tag == llvm::dwarf::DW_TAG_union_type))
        {
            tree_type = T_STRUCT;
            tree = TreeNode(name);
            if(tree) TreePop();
        }
        else if((n = array_size(resolved_die)) != 0)
        {
            tree_type = T_ARRAY;
            tree = TreeNode(name);
            if(tree) TreePop();
        }
        else
        {
            Dummy({ GetTreeNodeToLabelSpacing(), 1.f });
            SameLine(0.f, 0.f);
            TextUnformatted(name);
        }
        if(indent != 0.f) Unindent(indent);

        if(!local && IsItemHovered())
        {
            uint16_t addr = (uint16_t)(ptrdiff_t)(mem.begin -
                (text ? arduboy->cpu.prog.data() : arduboy->cpu.data.data()));
            BeginTooltip();
            Text("Address: 0x%04x%s", addr, text ? " [PROG]" : "");
            EndTooltip();
        }
    }

    if(TableSetColumnIndex(1))
    {
        t = dwarf_value_string(die, mem, bit_offset, bit_size);
        if(t.empty())
            TextDisabled("???");
        else
        {
            TextUnformatted(t.c_str());
            if(IsItemHovered() && BeginTooltip())
            {
                t = dwarf_value_string(die, mem, bit_offset, bit_size, dwarf_value_base::hex);
                Text("Hex: %s", t.c_str());
                t = dwarf_value_string(die, mem, bit_offset, bit_size, dwarf_value_base::bin);
                Text("Bin: %s", t.c_str());
                EndTooltip();
            }
        }
    }

    if(TableSetColumnIndex(2))
    {
        t = dwarf_type_string(die);
        if(t.empty())
            TextDisabled("???");
        else
        {
            if(bit_size != 0)
                t += fmt::format(" : {}", bit_size);
            TextUnformatted(t.c_str());
        }
    }

    if(tree)
    {
        if(tree_type == T_STRUCT)
        {
            int child_id = 0;
			Indent(GetTreeNodeToLabelSpacing());
            for(auto const& child : dwarf_members(resolved_die))
            {
                do_var_row(
                    child_id++, dwarf_name(child.die).c_str(), false, local,
                    mem.offset(child.offset), text, dwarf_type(child.die),
                    child.bit_offset, child.bit_size);
            }
			Unindent(GetTreeNodeToLabelSpacing());
        }
        if(tree_type == T_ARRAY)
        {
            auto type = dwarf_type(resolved_die);
            auto size = dwarf_size(type);
            std::string name;

            //n = std::min<uint32_t>(n, 8);

            for(uint32_t i = 0; i < n; ++i)
            {
                auto child_mem = mem.offset(i * size);
                auto v = dwarf_array_index(resolved_die, i);
                if(v.empty()) continue;
                name.clear();
                for(auto n : v)
                    name += fmt::format("[{}]", n);
                //name = fmt::format("[{}]", i);
                Indent(GetTreeNodeToLabelSpacing());
                do_var_row((int)i, name.c_str(), false, local, child_mem, text, type);
                Unindent(GetTreeNodeToLabelSpacing());
            }
        }
    }
    PopID();

    return remove;
}

// return true if user wants to delete
static bool do_global(
    int id,
    std::string const& name, absim::elf_data_t::global_t const& g)
{
    using namespace ImGui;

    bool remove = false;
    auto* dwarf = arduboy->elf->dwarf_ctx.get();
    auto* cu = dwarf->getCompileUnitForOffset(g.cu_offset);
    if(!cu) return true;

    auto type = cu->getDIEForOffset(g.type);

    dwarf_span mem;
    if(g.text)
        mem = to_dwarf_span(arduboy->cpu.prog);
    else
        mem = to_dwarf_span(arduboy->cpu.data);

    return do_var_row(id, name.c_str(), true, false, mem.offset(g.addr), g.text, type);
}

struct local_var_t
{
    std::string name;
    uint32_t id;
    llvm::DWARFDie type;
    dwarf_var_data var_data;
};

static void gather_locals(
    llvm::DWARFDie die,
    std::vector<local_var_t>& locals)
{
    if(!die.isValid()) return;

    uint64_t pc = uint64_t(arduboy->cpu.pc) * 2;

    llvm::DWARFAddressRangesVector ranges;
    ranges.push_back({ 0, UINT64_MAX });
    if(auto eranges = die.getAddressRanges())
        ranges = std::move(*eranges);

    for(auto child : die)
    {
        if(child.getTag() == llvm::dwarf::DW_TAG_inlined_subroutine ||
            child.getTag() == llvm::dwarf::DW_TAG_lexical_block)
        {
            gather_locals(child, locals);
        }
        else if(child.getTag() == llvm::dwarf::DW_TAG_variable ||
            child.getTag() == llvm::dwarf::DW_TAG_formal_parameter)
        {
            if(ranges.empty()) continue;
            auto name = dwarf_name(child);
            auto tag = child.getTag();
            if(auto elocs = child.getLocations(llvm::dwarf::DW_AT_location))
            {
                for(auto const& loc : *elocs)
                {
                    bool valid = false;
                    for(auto const& range : ranges)
                        if(pc >= range.LowPC && pc < range.HighPC)
                            valid = true;
                    if(!valid) continue;
                    if(loc.Range && (pc < loc.Range->LowPC || pc >= loc.Range->HighPC))
                        continue;
                    auto type = dwarf_type(child);
                    auto vd = dwarf_evaluate_location(type, llvm::toStringRef(loc.Expr));
                    if(!vd.data.empty())
                    {
                        locals.resize(locals.size() + 1);
                        auto& local = locals.back();
                        local.name = dwarf_name(child);
                        local.id = (uint32_t)child.getOffset();
                        local.type = type;
                        local.var_data = std::move(vd);
                        break;
                    }
                }
            }
        }
    }
}

void window_locals(bool& open)
{
    using namespace ImGui;
    if(!open) return;
    SetNextWindowSize({ 400 * pixel_ratio, 400 * pixel_ratio }, ImGuiCond_FirstUseEver);
    ImGuiWindowFlags wflags = 0;
    if(Begin("Locals", &open, wflags) &&
        arduboy->cpu.decoded && arduboy->elf && arduboy->paused)
    {
        auto& e = *arduboy->elf;
        auto& dwarf = *e.dwarf_ctx;
        uint64_t pc = uint64_t(arduboy->cpu.pc) * 2;

        auto dies = dwarf.getDIEsForAddress(pc);
        std::vector<local_var_t> locals;
        gather_locals(dies.FunctionDIE, locals);

        ImGuiTableFlags flags = 0;
        flags |= ImGuiTableFlags_ScrollY;
        flags |= ImGuiTableFlags_Resizable;
        flags |= ImGuiTableFlags_RowBg;
        flags |= ImGuiTableFlags_BordersOuter;
        flags |= ImGuiTableFlags_BordersInnerV;
        if(BeginTable("##globals", 3, flags, { -1, -1 }))
        {
            TableSetupColumn("Name",
                ImGuiTableColumnFlags_WidthStretch);
            TableSetupColumn("Value",
                ImGuiTableColumnFlags_WidthStretch);
            TableSetupColumn("Type",
                ImGuiTableColumnFlags_WidthStretch);
            TableHeadersRow();

            for(auto const& local : locals)
            {
                do_var_row(
                    (int)local.id, local.name.c_str(), true, true,
                    to_dwarf_span(local.var_data.data), false, local.type);
            }

            EndTable();
        }
    }
    End();
}

void window_globals(bool& open)
{
    using namespace ImGui;
    if(!open) return;
    SetNextWindowSize({ 400 * pixel_ratio, 400 * pixel_ratio }, ImGuiCond_FirstUseEver);
    ImGuiWindowFlags wflags = 0;
    if(Begin("Globals", &open, wflags) && arduboy->cpu.decoded && arduboy->elf)
    {
        if(Button("Add variable..."))
            OpenPopup("##addvar");
        SameLine();
        if(Button("Add PROGMEM object..."))
            OpenPopup("##addprog");
        SameLine();
        if(Button("Add IO register..."))
            OpenPopup("##addio");

        if(BeginPopup("##addvar"))
        {
            for(auto const& kv : arduboy->elf->globals)
            {
                auto const& name = kv.first;
                auto const& g = kv.second;
                
                if(g.text || g.addr < 0x100) continue;
                if(MenuItem(name.c_str()))
                    watches.push_back(name);
            }
            EndPopup();
        }
        if(BeginPopup("##addprog"))
        {
            for(auto const& kv : arduboy->elf->globals)
            {
                auto const& name = kv.first;
                auto const& g = kv.second;
                if(!g.text) continue;
                if(MenuItem(name.c_str()))
                    watches.push_back(name);
            }
            EndMenu();
        }
        if(BeginPopup("##addio"))
        {
            for(auto const& kv : arduboy->elf->globals)
            {
                auto const& name = kv.first;
                auto const& g = kv.second;
                if(g.text || g.addr >= 0x100) continue;
                if(MenuItem(name.c_str()))
                    watches.push_back(name);
            }
            EndMenu();
        }

        ImGuiTableFlags flags = 0;
        flags |= ImGuiTableFlags_ScrollY;
        flags |= ImGuiTableFlags_Resizable;
        flags |= ImGuiTableFlags_RowBg;
        flags |= ImGuiTableFlags_BordersOuter;
        flags |= ImGuiTableFlags_BordersInnerV;
        if(BeginTable("##globals", 3, flags, { -1, -1 }))
        {
            TableSetupColumn("Name",
                ImGuiTableColumnFlags_WidthStretch);
            TableSetupColumn("Value",
                ImGuiTableColumnFlags_WidthStretch);
            TableSetupColumn("Type",
                ImGuiTableColumnFlags_WidthStretch);
            TableHeadersRow();

            for(size_t i = 0; i < watches.size(); ++i)
            {
                auto const& name = watches[i];
                auto it = arduboy->elf->globals.find(name);
                if(it == arduboy->elf->globals.end())
                {
                    watches.erase(watches.begin() + i);
                    --i;
                    continue;
                }
                auto const& g = it->second;
                bool remove = do_global((int)i, name, g);
                if(remove)
                {
                    watches.erase(watches.begin() + i);
                    --i;
                    continue;
                }
            }

            EndTable();
        }

    }
    End();
}

#endif
