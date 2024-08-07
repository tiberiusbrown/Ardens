#include "absim_dwarf.hpp"

#ifdef ARDENS_LLVM

#include "common.hpp"

#include <assert.h>

namespace absim
{

struct stack_item
{
    uint16_t data;
    bool is_addr;
};

struct expr_stack
{
    std::vector<stack_item> data;
    void push(stack_item x) { data.push_back(x); }
    template<class T> void push_data(T x) { data.push_back({ (uint16_t)x, false }); }
    template<class T> void push_addr(T x) { data.push_back({ (uint16_t)x, true }); }
    stack_item pop() { auto x = data.back(); data.pop_back(); return x; }
    stack_item top() { return data.back(); }
    bool has_two() const { return data.size() >= 2; }
    bool not_two() const { return !has_two(); }
    bool empty() const { return data.empty(); }
    stack_item pick(size_t i) const { return data[data.size() - i - 1]; }
    size_t size() const { return data.size(); }
};

static uint64_t regval(uint64_t i)
{
    if(i < 31)
    {
        uint64_t x = arduboy.cpu.data[i + 1];
        x = (x << 8) + arduboy.cpu.data[i];
        return x;
    }
    else if(i == 31)
        return arduboy.cpu.data[i];
    else if(i == 32)
        return arduboy.cpu.sp();
    else
        return -1;
}

#define CHECK(cond__) do { \
        assert(cond__) ; \
        if(!(cond__)) return {}; \
    } while(0)

dwarf_var_data dwarf_evaluate_location(
    llvm::DWARFDie type,
    llvm::StringRef loc)
{
    // TODO: use DWARFExpression::iterator::skipBytes for branching
    // TODO: fbreg

    dwarf_var_data vd;
    auto& d = vd.data;
    size_t i = 0;

    d.resize(dwarf_size(type));
    if(d.empty()) return {};
    vd.unavailable.resize(d.size());

    expr_stack stack;

    auto const& cpu = arduboy.cpu;

    auto extractor = llvm::DataExtractor(loc,
        type.getDwarfUnit()->getContext().isLittleEndian(), 0);
    llvm::DWARFExpression expr(extractor,
        type.getDwarfUnit()->getAddressByteSize(),
        type.getDwarfUnit()->getFormParams().Format);

    for(auto const& op : expr)
    {
        using namespace llvm::dwarf;
        if(op.isError())
            return {};
        auto code = op.getCode();
        if(code >= DW_OP_lit0 && code <= DW_OP_lit31)
        {
            stack.push_data(code - DW_OP_lit0);
        }
        else if(
            code == DW_OP_const1u ||
            code == DW_OP_const1s ||
            code == DW_OP_const2u ||
            code == DW_OP_const2s ||
            code == DW_OP_const4u ||
            code == DW_OP_const4s ||
            code == DW_OP_const8u ||
            code == DW_OP_const8s ||
            code == DW_OP_constu ||
            code == DW_OP_consts ||
            0)
        {
            stack.push_data(op.getRawOperand(0));
        }
        else if(code >= DW_OP_breg0 && code <= DW_OP_breg31)
        {
            stack.push_addr(regval(code - DW_OP_breg0) + op.getRawOperand(0));
        }
        else if(code >= DW_OP_reg0 && code <= DW_OP_reg31)
        {
            stack.push_addr(code - DW_OP_reg0);
        }
        else if(code == DW_OP_bregx)
        {
            auto i = op.getRawOperand(0);
            auto t = op.getRawOperand(1);
            CHECK(i <= 32);
            stack.push_data(regval(i) + t);
        }
        else if(code == DW_OP_fbreg)
        {
            // TODO
            return {};
        }
        else if(code == DW_OP_dup)
        {
            CHECK(!stack.empty());
            stack.push(stack.top());
        }
        else if(code == DW_OP_drop)
        {
            CHECK(!stack.empty());
            stack.pop();
        }
        else if(code == DW_OP_pick)
        {
            auto i = op.getRawOperand(0);
            CHECK(i < stack.size());
            stack.push(stack.pick(i));
        }
        else if(code == DW_OP_swap)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push(a);
            stack.push(b);
        }
        else if(code == DW_OP_rot)
        {
            CHECK(stack.size() >= 3);
            auto a = stack.pop();
            auto b = stack.pop();
            auto c = stack.pop();
            stack.push(a);
            stack.push(c);
            stack.push(b);
        }
        else if(code == DW_OP_deref)
        {
            CHECK(!stack.empty());
            auto t = stack.pop();
            CHECK(t.data + 1 < cpu.data.size());
            uint64_t x = cpu.data[t.data + 1];
            x = (x << 8) + cpu.data[t.data];
            stack.push_data(x);
        }
        else if(code == DW_OP_deref_size)
        {
            CHECK(!stack.empty());
            auto t = stack.pop();
            auto n = op.getRawOperand(0);
            CHECK(n <= 2);
            CHECK(t.data + n <= cpu.data.size());
            uint64_t x = 0;
            if(n == 2) x = cpu.data[t.data + 1];
            x = (x << 8) + cpu.data[t.data];
            stack.push_data(x);
        }
        else if(code == DW_OP_abs)
        {
            CHECK(!stack.empty());
            auto td = stack.pop();
            int16_t t = (int16_t)td.data;
            stack.push({ uint16_t(t < 0 ? -t : t), td.is_addr });
        }
        else if(code == DW_OP_neg)
        {
            CHECK(!stack.empty());
            auto a = stack.pop();
            stack.push({ uint16_t(-a.data), a.is_addr });
        }
        else if(code == DW_OP_not)
        {
            CHECK(!stack.empty());
            auto a = stack.pop();
            stack.push({ uint16_t(~a.data), a.is_addr });
        }
        else if(code == DW_OP_and)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push({ uint16_t(a.data & b.data), a.is_addr || b.is_addr });
        }
        else if(code == DW_OP_or)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push({ uint16_t(a.data | b.data), a.is_addr || b.is_addr });
        }
        else if(code == DW_OP_xor)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push({ uint16_t(a.data ^ b.data), a.is_addr || b.is_addr });
        }
        else if(code == DW_OP_plus)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push({ uint16_t(a.data + b.data), a.is_addr || b.is_addr });
        }
        else if(code == DW_OP_plus_uconst)
        {
            CHECK(!stack.empty());
            auto a = stack.pop();
            stack.push({ uint16_t(a.data + op.getRawOperand(0)), a.is_addr });
        }
        else if(code == DW_OP_mul)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push({ uint16_t((int16_t)a.data * (int16_t)b.data),
                a.is_addr || b.is_addr });
        }
        else if(code == DW_OP_minus)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push({ uint16_t(b.data - a.data), a.is_addr || b.is_addr });
        }
        else if(code == DW_OP_mod)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            CHECK(a.data != 0);
            stack.push({ uint16_t(b.data % a.data), a.is_addr || b.is_addr });
        }
        else if(code == DW_OP_shl)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            CHECK(a.data != 0);
            stack.push({ uint16_t(b.data << a.data), a.is_addr || b.is_addr });
        }
        else if(code == DW_OP_shr)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            CHECK(a.data != 0);
            stack.push({ uint16_t(b.data >> a.data), a.is_addr || b.is_addr });
        }
        else if(code == DW_OP_shra)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            CHECK(a.data != 0);
            stack.push({ uint16_t((int16_t)b.data >> a.data), a.is_addr || b.is_addr });
        }
        else if(code == DW_OP_le)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push_data((int16_t)b.data <= (int16_t)a.data);
        }
        else if(code == DW_OP_ge)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push_data((int16_t)b.data >= (int16_t)a.data);
        }
        else if(code == DW_OP_eq)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push_data((int16_t)b.data == (int16_t)a.data);
        }
        else if(code == DW_OP_lt)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push_data((int16_t)b.data < (int16_t)a.data);
        }
        else if(code == DW_OP_gt)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push_data((int16_t)b.data > (int16_t)a.data);
        }
        else if(code == DW_OP_ne)
        {
            CHECK(stack.has_two());
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push_data((int16_t)b.data != (int16_t)a.data);
        }
        else if(code == DW_OP_nop)
        {
            // nothing
        }
        else if(code == DW_OP_stack_value)
        {
            stack.data.back().is_addr = false;
        }
        else if(code == DW_OP_piece)
        {
            auto n = op.getRawOperand(0);
            if(stack.empty())
            {
                // TODO: unavailable
                for(size_t j = i; j < i + n && j < d.size(); ++j)
                    vd.unavailable[j] = 0xff;
                i += n;
            }
            else
            {
                auto t = stack.pop();
                uint16_t data = t.data;
                while(n != 0)
                {
                    uint8_t byte;
                    if(t.is_addr)
                    {
                        byte = 0;
                        if(data < cpu.data.size())
                            byte = cpu.data[data++];
                    }
                    else
                    {
                        byte = (uint8_t)data;
                        data >>= 8;
                    }
                    if(i < d.size())
                        d[i] = byte;
                    --n;
                    ++i;
                }
            }
        }
        else if(code == DW_OP_addr)
        {
            auto t = op.getRawOperand(0);
            //t -= 0x800000;
            stack.push_addr(t);
        }
        else
            return {};
    }


    if(!stack.empty())
    {
        auto t = stack.pop();
        uint16_t data = t.data;
        while(i < d.size())
        {
            uint8_t byte;
            if(t.is_addr)
            {
                byte = 0;
                if(data < cpu.data.size())
                    byte = cpu.data[data++];
            }
            else
            {
                byte = (uint8_t)data;
                data >>= 8;
            }
            d[i] = byte;
            ++i;
        }
    }

    return vd;
}

}

#endif
