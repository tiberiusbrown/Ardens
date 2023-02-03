#include "absim.hpp"

#include "imgui.h"

#include <algorithm>

extern absim::arduboy_t arduboy;

static absim::elf_data_t::frame_info_t::unwind_t const* find_unwind(uint16_t addr)
{
	auto const& frames = arduboy.elf->frames;

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
	auto const& cpu = arduboy.cpu;
	if(addr + 1 >= cpu.data.size()) return 0;
	uint16_t lo = cpu.data[addr + 0];
	uint16_t hi = cpu.data[addr + 1];
	return lo | (hi << 8);
}

static void window_call_stack_contents()
{
	using namespace ImGui;
    auto const& cpu = arduboy.cpu;
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

	ImGuiTableFlags flags = 0;
	flags |= ImGuiTableFlags_ScrollY;
	if(BeginTable("##callstack", 2, flags))
	{
        TableSetupColumn("Address",
            ImGuiTableColumnFlags_WidthFixed,
            CalcTextSize("0x0000").x);
        TableSetupColumn("Symbol");
		for(int depth = 0; depth < 32; ++depth)
		{
            if(addr >= cpu.last_addr) break;

			TableNextRow();

			TableSetColumnIndex(0);
			Text("0x%04x", addr);

			TableSetColumnIndex(1);
			auto const* sym = arduboy.symbol_for_prog_addr(addr);
			if(sym)
			{
			    TextUnformatted(sym->name.c_str());
			}

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

		EndTable();
	}
}

void window_call_stack(bool& open)
{
	using namespace ImGui;
	if(!open) return;

	SetNextWindowSize({ 200, 100 }, ImGuiCond_FirstUseEver);
	if(Begin("Call Stack", &open) && arduboy.cpu.decoded && arduboy.elf)
	{
		window_call_stack_contents();
	}
	End();
}
