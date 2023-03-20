#include "absim.hpp"

#include "imgui.h"

#include <algorithm>
#include <vector>

#include <fmt/format.h>

extern std::unique_ptr<absim::arduboy_t> arduboy;
extern int disassembly_scroll_addr;

#if 0
static absim::elf_data_t::frame_info_t::unwind_t const* find_unwind(uint16_t addr)
{
	auto const& frames = arduboy->elf->frames;

	// find frame
	auto fit = std::find_if(frames.begin(), frames.end(),
		[&](auto const& x) { return addr >= x.addr_lo && addr < x.addr_hi; });
	if(fit == frames.end()) return nullptr;
	auto const& fi = *fit;

	auto uit = std::find_if(fi.unwinds.rbegin(), fi.unwinds.rend(),
		[&](auto const& x) { return addr >= x.addr; });
	if(uit == fi.unwinds.rend()) return nullptr;

	return &*uit;
}

static uint16_t cpu_word(uint16_t addr)
{
	auto const& cpu = arduboy->cpu;
	if(addr + 1 >= cpu.data.size()) return 0;
	uint16_t lo = cpu.data[addr + 0];
	uint16_t hi = cpu.data[addr + 1];
	return lo | (hi << 8);
}

std::vector<uint16_t> get_call_stack()
{
    auto const& cpu = arduboy->cpu;
    auto addr = cpu.pc * 2;

    std::array<uint8_t, 34> regs;
    for(int i = 0; i < 32; ++i)
        regs[i] = cpu.data[i];
    regs[32] = cpu.data[0x5d];
    regs[33] = cpu.data[0x5e];

    auto word = [&](uint16_t addr) -> uint16_t {
        uint16_t lo = regs[addr + 0];
        uint16_t hi = regs[addr + 1];
        return lo | (hi << 8);
    };

    std::vector<uint16_t> r;
    if(!arduboy->elf) return r;

    for(int depth = 0; depth < 32; ++depth)
    {
        if(addr >= cpu.last_addr) break;

        r.push_back(addr);

        auto const* u = find_unwind(addr);
        if(!u) break;
        if(u->cfa_reg > 32) break;

        uint16_t cfa = word(u->cfa_reg) + u->cfa_offset;
        uint16_t ret = cpu_word(cfa + u->ra_offset);

        ret = (ret >> 8) | (ret << 8);
        ret *= 2;

        for(int i = 0; i < 32; ++i)
        {
            auto off = u->reg_offsets[i];
            if(off == INT16_MAX) continue;
            regs[i] = cpu_word(cfa + u->reg_offsets[i]);
        }

        regs[32] = uint8_t(cfa >> 0);
        regs[33] = uint8_t(cfa >> 8);

        addr = ret;
    }

    return r;
}
#endif

static void window_call_stack_contents()
{
	using namespace ImGui;

    //auto call_stack = get_call_stack();

	ImGuiTableFlags flags = 0;
	flags |= ImGuiTableFlags_ScrollY;
	flags |= ImGuiTableFlags_RowBg;
    std::string name;
	if(BeginTable("##callstack", 1, flags))
	{
        //for(auto addr : call_stack)
        for(int i = (int)arduboy->cpu.num_stack_frames; i >= 0; --i)
        {
            uint16_t addr = (i < (int)arduboy->cpu.num_stack_frames) ?
                arduboy->cpu.stack_frames[i].pc * 2 :
                arduboy->cpu.pc * 2;
            if(addr >= arduboy->cpu.last_addr) break;

            TableNextRow();

            auto const* sym = arduboy->symbol_for_prog_addr(addr);
            if(sym)
                name = fmt::format("{:#06x} {}", addr, sym->name);
            else
                name = fmt::format("{:#06x}", addr);

            TableSetColumnIndex(0);
            if(Selectable(name.c_str()))
                disassembly_scroll_addr = (int)addr;
        }

		EndTable();
	}
}

void window_call_stack(bool& open)
{
	using namespace ImGui;
	if(!open) return;

	SetNextWindowSize({ 200, 100 }, ImGuiCond_FirstUseEver);
	if(Begin("Call Stack", &open) && arduboy->cpu.decoded)
	{
		window_call_stack_contents();
	}
	End();
}
