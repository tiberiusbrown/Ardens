#ifdef ARDENS_LLVM

#include "common.hpp"
#include "dwarf.hpp"

#include <fmt/format.h>

std::vector<dwarf_member_t> dwarf_members(llvm::DWARFDie die)
{
    using namespace llvm;
    using namespace llvm::dwarf;

    std::vector<dwarf_member_t> r;

    if(die.getTag() != DW_TAG_structure_type && die.getTag() != DW_TAG_union_type)
        return r;

    for(auto child : die)
    {
        if(child.getTag() != DW_TAG_member) continue;
        uint32_t offset = 0;
        uint32_t bit_offset = 0;
        uint32_t bit_size = 0;
        if(!dwarf_member_addr(offset, bit_offset, bit_size, child)) continue;
        if(dwarf_name(child).empty())
        {
            // anonymous union
            auto type = dwarf_type(child);
            if(!type.isValid()) continue;
            if(type.getTag() == DW_TAG_union_type)
            {
                for(auto gchild : type)
                {
                    if(gchild.getTag() != DW_TAG_member) continue;
                    r.push_back({ offset, bit_offset, bit_size, gchild });
                }
            }
            // anonymous struct
            if(type.getTag() == DW_TAG_structure_type)
            {
                for(auto gchild : type)
                {
                    if(gchild.getTag() != DW_TAG_member) continue;
                    uint32_t toff = offset;
                    if(!dwarf_member_addr(toff, bit_offset, bit_size, gchild)) continue;
                    r.push_back({ toff, bit_offset, bit_size, gchild });
                }
            }
            continue;
        }
        r.push_back({ offset, bit_offset, bit_size, child });
    }

    return r;
}

bool dwarf_member_addr(
    uint32_t& addr, uint32_t& bit_offset, uint32_t& bit_size,
    llvm::DWARFDie die)
{
    if(auto a = die.find(llvm::dwarf::DW_AT_bit_size))
        if(auto v = a->getAsUnsignedConstant())
            bit_size = (uint32_t)v.value();
    if(bit_size != 0)
    {
        if(auto t = die.find(llvm::dwarf::DW_AT_bit_offset))
            if(auto v = t->getAsUnsignedConstant())
                bit_offset = dwarf_size(die) * 8 - bit_size - (uint32_t)v.value();
        if(auto t = die.find(llvm::dwarf::DW_AT_data_bit_offset))
            if(auto v = t->getAsUnsignedConstant())
                bit_offset = (uint32_t)v.value();
    }
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

std::vector<uint32_t> dwarf_array_bounds(llvm::DWARFDie die)
{
    using namespace llvm;
    using namespace llvm::dwarf;

    std::vector<uint32_t> r;

    if(die.getTag() != DW_TAG_array_type)
        return r;

    for(auto child : die)
    {
        if(child.getTag() != DW_TAG_subrange_type)
            continue;

        size_t n = 0;
        if(auto elem_count_attr = child.find(DW_AT_count))
            if(auto elem_count = elem_count_attr->getAsUnsignedConstant())
                n = *elem_count;
        if(auto upper_bound_attr = child.find(DW_AT_upper_bound))
        {
            if(auto upper_bound = upper_bound_attr->getAsUnsignedConstant())
            {
                int64_t lower_bound = 0;
                if(auto lower_bound_attr = child.find(DW_AT_lower_bound))
                    lower_bound = lower_bound_attr->getAsUnsignedConstant().value_or(0);
                n = *upper_bound - lower_bound + 1;
            }
        }
        r.push_back(n);
    }

    return r;
}

std::vector<uint32_t> dwarf_array_index(llvm::DWARFDie die, uint32_t index)
{
    auto v = dwarf_array_bounds(die);
    if(v.empty()) return {};

    {
        uint32_t t = 1;
        for(auto n : v)
            t *= n;
        if(t != 0 && index >= t)
            return {};
    }

    for(uint32_t i = uint32_t(v.size()); i != 0; )
    {
        --i;
        uint32_t t = v[i];
        if(t == 0)
        {
            v[i] = index;
            break;
        }
        v[i] = index % t;
        index /= t;
    }

    return v;
}

uint32_t dwarf_size(llvm::DWARFDie die)
{
    // DWARFDie::getTypeSize has a bug where it reads array upper bounds as signed
    // when they should be unsigned. reimplement it here

    // return (uint32_t)die.getTypeSize(2).value_or(0);

    using namespace llvm;
    using namespace llvm::dwarf;

    if(!die.isValid()) return 0;

    if(auto SizeAttr = die.find(DW_AT_byte_size))
        if(auto Size = SizeAttr->getAsUnsignedConstant())
            return (uint32_t)Size.value();

    constexpr uint32_t PointerSize = 2;

    switch(die.getTag())
    {
    case DW_TAG_pointer_type:
    case DW_TAG_reference_type:
    case DW_TAG_rvalue_reference_type:
        return PointerSize;
    case DW_TAG_ptr_to_member_type:
        if(DWARFDie BaseType = die.getAttributeValueAsReferencedDie(DW_AT_type))
            if(BaseType.getTag() == DW_TAG_subroutine_type)
                return 2 * PointerSize;
        return PointerSize;
    case DW_TAG_array_type:
    {
        DWARFDie BaseType = die.getAttributeValueAsReferencedDie(DW_AT_type);
        if(!BaseType)
            return 0;
        auto BaseSize = BaseType.getTypeSize(PointerSize);
        if(!BaseSize)
            return 0;
        uint64_t Size = *BaseSize;
        for(DWARFDie Child : die) {
            if(Child.getTag() != DW_TAG_subrange_type)
                continue;

            if(auto ElemCountAttr = Child.find(DW_AT_count))
                if(auto ElemCount = ElemCountAttr->getAsUnsignedConstant())
                    Size *= *ElemCount;
            if(auto UpperBoundAttr = Child.find(DW_AT_upper_bound))
            {
                if(auto UpperBound = UpperBoundAttr->getAsUnsignedConstant())
                {
                    int64_t LowerBound = 0;
                    if(auto LowerBoundAttr = Child.find(DW_AT_lower_bound))
                        LowerBound = LowerBoundAttr->getAsUnsignedConstant().value_or(0);
                    Size *= *UpperBound - LowerBound + 1;
                }
            }
        }
        return (uint32_t)Size;
    }
    default:
        if(DWARFDie BaseType = die.getAttributeValueAsReferencedDie(DW_AT_type))
            return dwarf_size(BaseType);
        break;
    }
    return 0;
}

std::string dwarf_name(llvm::DWARFDie die)
{
    char const* name = die.getShortName();
    while(!name && die.isValid())
    {
        die = dwarf_type(die);
        name = die.getShortName();
    }
    if(!name) name = "";
    return name;
}

llvm::DWARFDie dwarf_type(llvm::DWARFDie die)
{
    auto type = die.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    if(type.isValid())
        return type;
    type = die.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_abstract_origin);
    if(type.isValid())
        return dwarf_type(type);
    return type;
}

static std::string recurse_varname(llvm::DWARFDie die)
{
    if(!die.isValid())
        return "void";
    switch(die.getTag())
    {
    case llvm::dwarf::DW_TAG_base_type:
    case llvm::dwarf::DW_TAG_typedef:
    case llvm::dwarf::DW_TAG_enumeration_type:
        return dwarf_name(die);
    case llvm::dwarf::DW_TAG_structure_type:
        return fmt::format("struct {}", dwarf_name(die));
    case llvm::dwarf::DW_TAG_union_type:
        return fmt::format("union {}", dwarf_name(die));
    case llvm::dwarf::DW_TAG_const_type:
        return recurse_varname(dwarf_type(die)) + " const";
    case llvm::dwarf::DW_TAG_volatile_type:
        return recurse_varname(dwarf_type(die)) + " volatile";
    case llvm::dwarf::DW_TAG_pointer_type:
    {
        auto type = dwarf_type(die);
        if(type.isValid() && type.getTag() == llvm::dwarf::DW_TAG_subroutine_type)
        {
            std::string r = recurse_varname(dwarf_type(type));
            r += "(*)(";
            bool found = false;
            bool first = true;
            for(auto p : type.children())
            {
                if(p.getTag() != llvm::dwarf::DW_TAG_formal_parameter)
                    continue;
                if(!first) r += ", ";
                first = false;
                r += recurse_varname(p);
            }
            if(!found) r += "void";
            r += ")";
            return r;
        }
        return recurse_varname(type) + "*";
    }
    case llvm::dwarf::DW_TAG_reference_type:
        return recurse_varname(dwarf_type(die)) + "&";
    case llvm::dwarf::DW_TAG_array_type:
    {
        std::string brackets;
        for(auto child : die.children())
        {
            if(!child.isValid()) break;

            int n = -1;
            if(child.getTag() == llvm::dwarf::DW_TAG_subrange_type)
            {
                if(auto tf = child.find(llvm::dwarf::DW_AT_upper_bound))
                    if(auto tn = tf.value().getAsUnsignedConstant())
                        n = tn.value();
            }
            if(n < 0)
                brackets += "[]";
            else
                brackets += fmt::format("[{}]", n + 1);
        }
        return recurse_varname(dwarf_type(die)) + brackets;
    }
    default:
        break;
    }
    return "<unknown>";
}

std::string dwarf_type_string(llvm::DWARFDie die)
{
    return recurse_varname(die);
}

static std::string recurse_value(
    llvm::DWARFDie die, dwarf_span mem,
    uint32_t bit_offset = 0, uint32_t bit_size = 0)
{
    if(!die.isValid())
        return "";
    switch(die.getTag())
    {
    case llvm::dwarf::DW_TAG_const_type:
    case llvm::dwarf::DW_TAG_volatile_type:
    case llvm::dwarf::DW_TAG_typedef:
        return recurse_value(dwarf_type(die), mem, bit_offset, bit_size);
    case llvm::dwarf::DW_TAG_structure_type:
    case llvm::dwarf::DW_TAG_union_type:
    {
        std::string r;
        r += "{";
        bool first = true;
        uint32_t size = 0;
        for(auto const& child : dwarf_members(die))
        {
            if(size >= 8)
            {
                r += ", ...";
                break;
            }
            if(!first) r += ", ";
            r += recurse_value(
                dwarf_type(child.die), mem.offset(child.offset),
                child.bit_offset, child.bit_size);
            first = false;
            size += dwarf_size(child.die);
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
        for(auto child : die.children())
        {
            if(child.getTag() == llvm::dwarf::DW_TAG_subrange_type)
            {
                if(auto tb = child.find(llvm::dwarf::DW_AT_upper_bound))
                    if(auto b = tb->getAsUnsignedConstant())
                        n *= ((int)b.value() + 1), more = false;
            }
        }
        auto type = dwarf_type(die);
        auto size = dwarf_size(type);
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
                r += recurse_value(type, mem.offset(size * i));
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
    case llvm::dwarf::DW_TAG_reference_type:
    case llvm::dwarf::DW_TAG_enumeration_type:
    {
        int bits = bit_size, enc = llvm::dwarf::DW_ATE_unsigned;
        bool is_enum =
            (die.getTag() == llvm::dwarf::DW_TAG_enumeration_type);
        auto old_die = die;
        if(is_enum) die = dwarf_type(die);
        if(bits == 0)
            if(auto a = die.find(llvm::dwarf::DW_AT_byte_size))
                if(auto v = a->getAsUnsignedConstant())
                    bits = (int)v.value() * 8;
        if(auto a = die.find(llvm::dwarf::DW_AT_encoding))
            if(auto v = a->getAsUnsignedConstant())
                enc = (int)v.value();
        if(bits == 0 || bits > 64)
            break;
        mem = mem.offset(bit_offset / 8);
        //addr += bit_offset / 8;
        bit_offset %= 8;
        int bytes = (bits + 7) / 8;
        if(bytes > mem.size()) break;
        uint64_t x = 0;

        // extract data bits from RAM
        // (may not be byte-aligned for bitfields)
        for(int i = 0, j = 0; i < bits; ++j)
        {
            uint8_t byte = mem[j];
            byte >>= bit_offset;
            if(bits - i < 8)
                byte &= ((1 << (bits - i)) - 1);
            x |= (uint64_t(byte) << i);
            i += (8 - bit_offset);
            bit_offset = 0;
        }

        if(die.getTag() == llvm::dwarf::DW_TAG_pointer_type ||
            die.getTag() == llvm::dwarf::DW_TAG_reference_type)
            return fmt::format("{:#06x}", x);
        if(is_enum)
        {
            for(auto child : old_die)
            {
                if(child.getTag() != llvm::dwarf::DW_TAG_enumerator)
                    continue;
                if(auto t = child.find(llvm::dwarf::DW_AT_const_value))
                    if(auto v = t.value().getAsUnsignedConstant())
                        if(v == x) return fmt::format("{} ({})", dwarf_name(child), x);
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
            //if(x >= 32 && x < 127)
            //    return fmt::format("'{}'", (char)x);
            //return fmt::format("(char){}", (int)x);
            return fmt::format("{}", (int8_t)x);
        case llvm::dwarf::DW_ATE_signed:
            if(bits < 64)
            {
                uint64_t mask = 1ull << (bits - 1);
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

std::string dwarf_value_string(
    llvm::DWARFDie die, uint32_t addr, bool prog,
    uint32_t bit_offset, uint32_t bit_size)
{
    dwarf_span mem;
    if(prog) mem = to_dwarf_span(arduboy->cpu.prog);
    else     mem = to_dwarf_span(arduboy->cpu.data);
    return recurse_value(die, mem.offset(addr), bit_offset, bit_size);
}

std::string dwarf_value_string(
    llvm::DWARFDie die, dwarf_span mem,
    uint32_t bit_offset, uint32_t bit_size)
{
    return recurse_value(die, mem, bit_offset, bit_size);
}

bool dwarf_find_primitive(llvm::DWARFDie die, uint32_t offset, dwarf_primitive_t& prim)
{
    if(!die.isValid()) return false;
    switch(die.getTag())
    {
    case llvm::dwarf::DW_TAG_const_type:
    case llvm::dwarf::DW_TAG_volatile_type:
        return dwarf_find_primitive(dwarf_type(die), offset, prim);
    case llvm::dwarf::DW_TAG_structure_type:
    case llvm::dwarf::DW_TAG_union_type:
        for(auto const& p : dwarf_members(die))
        {
            auto size = dwarf_size(p.die);
            if(!(offset >= p.offset && offset < p.offset + size)) continue;
            prim.expr += fmt::format(".{}", dwarf_name(p.die));
            prim.bit_offset = p.bit_offset;
            prim.bit_size = p.bit_size;
            return dwarf_find_primitive(dwarf_type(p.die), offset - p.offset, prim);
        }
        return false;
    case llvm::dwarf::DW_TAG_array_type:
    {
        auto type = dwarf_type(die);
        auto size = dwarf_size(type);
        if(size <= 0) break;
        size_t i = offset / size;
        offset %= size;
        prim.expr += fmt::format("[{}]", i);
        return dwarf_find_primitive(type, offset, prim);
    }
    default:
        prim.die = die;
        prim.offset = offset;
        return true;
    }
    return false;
}

std::string dwarf_function_args_string(llvm::DWARFDie die)
{
    std::string r;

    r += dwarf_name(die);
    r += "(";
    bool first = true;
    for(auto child : die)
    {
        if(child.getTag() != llvm::dwarf::DW_TAG_formal_parameter)
            continue;
        if(!first) r += ", ";
        first = false;
        r += dwarf_type_string(dwarf_type(child));
    }
    r += ")";
    return r;
}

int recurse_varname(std::string& expr, uint16_t offset, llvm::DWARFDie die)
{
    if(!die.isValid()) return -1;
    switch(die.getTag())
    {
    case llvm::dwarf::DW_TAG_const_type:
    case llvm::dwarf::DW_TAG_volatile_type:
        return recurse_varname(expr, offset, dwarf_type(die));
    case llvm::dwarf::DW_TAG_structure_type:
        for(auto const& p : dwarf_members(die))
        {
            auto size = dwarf_size(p.die);
            if(!(offset >= p.offset && offset < p.offset + size)) continue;
            expr += fmt::format(".{}", dwarf_name(p.die));
            return recurse_varname(expr, offset - p.offset, dwarf_type(p.die));
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
        return recurse_varname(expr, offset, type);
    }
    default:
        if(dwarf_size(die) == 1) return -1;
        break;
    }
    return offset;
}

#endif
