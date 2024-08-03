#include "common.hpp"

#include "imgui.h"

#include <algorithm>
#include <vector>

#include <fmt/format.h>

extern int disassembly_scroll_addr;

static void window_call_stack_contents()
{
	using namespace ImGui;

    //auto call_stack = get_call_stack();

	ImGuiTableFlags flags = 0;
	flags |= ImGuiTableFlags_ScrollY;
	flags |= ImGuiTableFlags_RowBg;
    std::string name;
    bool copy_button_pressed = SmallButton("Copy to Clipboard");
    std::string str;
	if(BeginTable("##callstack", 1, flags))
	{
        //for(auto addr : call_stack)
        for(int i = (int)arduboy.cpu.num_stack_frames; i >= 0; --i)
        {
            uint16_t addr = (i < (int)arduboy.cpu.num_stack_frames) ?
                arduboy.cpu.stack_frames[i].pc * 2 :
                arduboy.cpu.pc * 2;
            if(addr >= arduboy.cpu.last_addr) break;

            TableNextRow();

            auto const* sym = arduboy.symbol_for_prog_addr(addr);
            if(sym)
                name = fmt::format("{:#06x} {}", addr, sym->name);
            else
                name = fmt::format("{:#06x}", addr);

            uint16_t prev_sp = (i > 0) ?
                arduboy.cpu.stack_frames[i - 1].sp :
                uint16_t(arduboy.cpu.data.size() - 1);
            uint16_t curr_sp = (i < (int)arduboy.cpu.num_stack_frames) ?
                arduboy.cpu.stack_frames[i].sp :
                arduboy.cpu.sp();
            uint16_t frame_size = prev_sp - curr_sp;

            name = fmt::format("[{:3}] ", frame_size) + name;

            if(copy_button_pressed) str += name + "\n";

            name += fmt::format("##callstack{}", i);

            TableSetColumnIndex(0);
            if(Selectable(name.c_str()))
                disassembly_scroll_addr = (int)addr;
        }

		EndTable();
	}
    if(copy_button_pressed)
        platform_set_clipboard_text(str.c_str());
}

void window_call_stack(bool& open)
{
	using namespace ImGui;
	if(!open) return;

	SetNextWindowSize({ 200 * pixel_ratio, 100 * pixel_ratio }, ImGuiCond_FirstUseEver);
	if(Begin("Call Stack", &open) && arduboy.cpu.decoded && arduboy.paused)
	{
		window_call_stack_contents();
	}
	End();
}
