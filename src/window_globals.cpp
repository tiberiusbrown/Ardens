#include "absim.hpp"

#include "imgui.h"

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

#include <fmt/format.h>

extern std::unique_ptr<absim::arduboy_t> arduboy;

static bool show_io = false;

constexpr int MAX_VALUE_CHARS = 64;

static std::vector<std::string> watches;

static std::string die_name(llvm::DWARFDie die)
{
    char const* name = die.getShortName();
    if(!name) name = "";
    return name;
}

static llvm::DWARFDie type_die(llvm::DWARFDie die)
{
    return die.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
}

static std::string recurse_type(llvm::DWARFDie die)
{
    if(!die.isValid())
        return "";
    switch(die.getTag())
    {
    case llvm::dwarf::DW_TAG_base_type:
    case llvm::dwarf::DW_TAG_typedef:
    case llvm::dwarf::DW_TAG_enumeration_type:
        return die_name(die);
    case llvm::dwarf::DW_TAG_structure_type:
        return fmt::format("struct {}", die_name(die));
    case llvm::dwarf::DW_TAG_union_type:
        return fmt::format("union {}", die_name(die));
    case llvm::dwarf::DW_TAG_const_type:
        return recurse_type(type_die(die)) + " const";
    case llvm::dwarf::DW_TAG_volatile_type:
        return recurse_type(type_die(die)) + " volatile";
    case llvm::dwarf::DW_TAG_pointer_type:
    {
        auto type = type_die(die);
        if(type.getTag() == llvm::dwarf::DW_TAG_subroutine_type)
        {
            std::string r = recurse_type(type_die(type));
            r += "(*)(";
            bool found = false;
            bool first = true;
            for(auto p : type.children())
            {
                if(p.getTag() != llvm::dwarf::DW_TAG_formal_parameter)
                    continue;
                if(!first) r += ", ";
                first = false;
                r += recurse_type(p);
            }
            if(!found) r += "void";
            r += ")";
            return r;
        }
        return recurse_type(type) + "*";
    }
    case llvm::dwarf::DW_TAG_reference_type:
        return recurse_type(type_die(die)) + "&";
    case llvm::dwarf::DW_TAG_array_type:
    {
        auto child = die.getFirstChild();
        if(!child.isValid()) break;
        int n = -1;
        if(child.getTag() == llvm::dwarf::DW_TAG_subrange_type)
        {
            if(auto tf = child.find(llvm::dwarf::DW_AT_upper_bound))
                if(auto tn = tf.getValue().getAsUnsignedConstant())
                    n = tn.getValue();
            if(n < 0) break;
        }
        auto type = type_die(die);
        if(n < 0)
            return recurse_type(type) + "[]";
        return fmt::format("{}[{}]", recurse_type(type), n + 1);
    }
    default:
        break;
    }
    return "<unknown>";
}

static uint32_t type_size(llvm::DWARFDie die)
{
    return (uint32_t)die.getTypeSize(2).value_or(0);
}

static bool member_addr(uint32_t& addr, llvm::DWARFDie die)
{
    if(auto tloc = die.find(llvm::dwarf::DW_AT_data_member_location))
    {
        auto expr = *tloc->getAsBlock();
        auto* u = die.getDwarfUnit();
        llvm::DataExtractor data(llvm::StringRef(
            (char const*)expr.data(), expr.size()),
            u->getContext().isLittleEndian(), 0);
        llvm::DWARFExpression dexpr(
            data, u->getAddressByteSize(), u->getFormParams().Format);
        for(auto& op : dexpr)
        {
            if(op.getCode() != llvm::dwarf::DW_OP_plus_uconst)
                continue;
            addr += (uint32_t)op.getRawOperand(0);
            return true;
        }
    }
    return false;
}

static std::string recurse_value(uint32_t addr, bool text, llvm::DWARFDie die)
{
    if(!die.isValid())
        return "";
    switch(die.getTag())
    {
    case llvm::dwarf::DW_TAG_const_type:
    case llvm::dwarf::DW_TAG_volatile_type:
    case llvm::dwarf::DW_TAG_typedef:
        return recurse_value(addr, text, type_die(die));
    case llvm::dwarf::DW_TAG_structure_type:
    case llvm::dwarf::DW_TAG_union_type:
    {
        std::string r;
        r += "{";
        bool first = true;
        uint32_t size = 0;
        for(auto child : die.children())
        {
            if(size >= 8)
            {
                r += ", ...";
                break;
            }
            if(child.getTag() != llvm::dwarf::DW_TAG_member) continue;
            uint32_t child_addr = addr;
            if(die.getTag() != llvm::dwarf::DW_TAG_union_type)
                if(!member_addr(child_addr, child))
                    continue;
            if(!first) r += ", ";
            r += recurse_value(child_addr, text, type_die(child));
            first = false;
            size += type_size(child);
        }
        r += "}";
        return r;
    }
    case llvm::dwarf::DW_TAG_array_type:
    {
        bool more = true;
        int n = 1;
        std::string r;
        r += "{";
        if(die.hasChildren())
        {
            auto child = die.getFirstChild();
            if(child.getTag() == llvm::dwarf::DW_TAG_subrange_type)
            {
                if(auto tb = child.find(llvm::dwarf::DW_AT_upper_bound))
                    if(auto b = tb->getAsUnsignedConstant())
                        n = (int)b.getValue() + 1, more = false;
            }
        }
        auto type = type_die(die);
        auto size = type_size(type);
        if(size * n > 8)
        {
            more = size * (n + 1) >= 8;
            n = std::max<uint32_t>(1, (8 + size - 1) / size);
        }
        if(size > 0)
        {
            bool first = true;
            for(int i = 0; i < n; ++i)
            {
                if(!first) r += ", ";
                r += recurse_value(addr + size * i, text, type);
                first = false;
            }
        }
        if(more)
            r += ", ...";
        r += "}";
        return r;
    }
    case llvm::dwarf::DW_TAG_base_type:
    case llvm::dwarf::DW_TAG_pointer_type:
    case llvm::dwarf::DW_TAG_enumeration_type:
    {
        int bytes = 0, enc = llvm::dwarf::DW_ATE_unsigned;
        bool is_enum =
            (die.getTag() == llvm::dwarf::DW_TAG_enumeration_type);
        auto old_die = die;
        if(is_enum) die = type_die(die);
        if(auto a = die.find(llvm::dwarf::DW_AT_byte_size))
            if(auto v = a->getAsUnsignedConstant())
                bytes = (int)v.getValue();
        if(auto a = die.find(llvm::dwarf::DW_AT_encoding))
            if(auto v = a->getAsUnsignedConstant())
                enc = (int)v.getValue();
        if(bytes == 0 || bytes > 8)
            break;
        if(text && addr + bytes >= arduboy->cpu.prog.size())
            break;
        if(!text && addr + bytes >= arduboy->cpu.data.size())
            break;
        uint8_t const* d = text ?
            arduboy->cpu.prog.data() :
            arduboy->cpu.data.data();
        uint64_t x = 0;
        for(int i = bytes - 1; i >= 0; --i)
            x = (x << 8) + d[addr + i];
        if(die.getTag() == llvm::dwarf::DW_TAG_pointer_type)
            return fmt::format("{:#06x}", x);
        if(is_enum)
        {
            for(auto child : old_die)
            {
                if(child.getTag() != llvm::dwarf::DW_TAG_enumerator)
                    continue;
                if(auto t = child.find(llvm::dwarf::DW_AT_const_value))
                    if(auto v = t.getValue().getAsUnsignedConstant())
                        if(v == x) return fmt::format("{} ({})", die_name(child), x);
            }
            return fmt::format("<invalid> ({})", x);
        }
        switch(enc)
        {
        case llvm::dwarf::DW_ATE_boolean:
            return x != 0 ? "true" : "false";
        case llvm::dwarf::DW_ATE_address:
            return fmt::format("{:#06x}", x);
        case llvm::dwarf::DW_ATE_float:
            if(bytes == 4)
            {
                union { uint64_t x; float f; } u;
                u.x = x;
                return fmt::format("{}", u.f);
            }
            if(bytes == 8)
            {
                union { uint64_t x; double f; } u;
                u.x = x;
                return fmt::format("{}", u.f);
            }
            break;
        case llvm::dwarf::DW_ATE_signed_char:
            if(x >= 32 && x < 127)
                return fmt::format("'{}'", (char)x);
            return fmt::format("(char){}", (int)x);
        case llvm::dwarf::DW_ATE_signed:
            if(bytes < 8)
            {
                uint64_t mask = 1ull << (bytes * 8 - 1);
                if(x & mask)
                    x |= ~(mask - 1);
            }
            return fmt::format("{}", (int64_t)x);
        case llvm::dwarf::DW_ATE_unsigned:
        case llvm::dwarf::DW_ATE_unsigned_char:
            return fmt::format("{}", x);
        default: break;
        }
        return fmt::format("{:#x}", x);
        break;
    }
    default:
        break;
    }
    return "<unknown>";
}

static uint32_t array_size(llvm::DWARFDie& die)
{
    if(!die.hasChildren()) return 0;
    if(die.getTag() != llvm::dwarf::DW_TAG_array_type) return 0;
    auto child = die.getFirstChild();
    if(child.getTag() != llvm::dwarf::DW_TAG_subrange_type) return 0;
    if(auto tu = child.find(llvm::dwarf::DW_AT_upper_bound))
        if(auto u = tu->getAsUnsignedConstant())
            return (uint32_t)u.getValue() + 1;
    return 0;
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
            die = type_die(die);
        }
        else
            break;
    }
    return die;
}

static bool do_var_row(
    int id,
    char const* name, bool root,
    uint32_t addr, bool text, llvm::DWARFDie die)
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
        if(root)
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

        if(IsItemHovered())
        {
            BeginTooltip();
            Text("Address: 0x%04x%s", addr, text ? " [PROG]" : "");
            EndTooltip();
        }
    }

    if(TableSetColumnIndex(1))
    {
        t = recurse_value(addr, text, die);
        if(t.empty())
            TextDisabled("???");
        else
            TextUnformatted(t.c_str());
    }

    if(TableSetColumnIndex(2))
    {
        t = recurse_type(die);
        if(t.empty())
            TextDisabled("???");
        else
            TextUnformatted(t.c_str());
    }

    if(tree)
    {
        if(tree_type == T_STRUCT)
        {
            int child_id = 0;
            for(auto child : resolved_die.children())
            {
                if(child.getTag() != llvm::dwarf::DW_TAG_member) continue;
                uint32_t child_addr = addr;
                if(die.getTag() != llvm::dwarf::DW_TAG_union_type)
                    if(!member_addr(child_addr, child)) continue;
                char const* child_name = child.getShortName();
                if(!child_name) continue;
                Indent(GetTreeNodeToLabelSpacing());
                do_var_row(child_id++, child_name, false, child_addr, text, type_die(child));
                Unindent(GetTreeNodeToLabelSpacing());
            }
        }
        if(tree_type == T_ARRAY)
        {
            auto type = type_die(resolved_die);
            auto size = type_size(type);
            std::string name;

            //n = std::min<uint32_t>(n, 8);

            for(uint32_t i = 0; i < n; ++i)
            {
                uint32_t child_addr = addr + i * size;
                name = fmt::format("[{}]", i);
                Indent(GetTreeNodeToLabelSpacing());
                do_var_row((int)i, name.c_str(), false, child_addr, text, type);
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

    return do_var_row(id, name.c_str(), true, g.addr, g.text, type);
}

void window_globals(bool& open)
{
    using namespace ImGui;
    if(!open) return;
    SetNextWindowSize({ 400, 400 }, ImGuiCond_FirstUseEver);
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
