#include "imgui.h"
#include "common.hpp"

void window_serial(bool& open)
{
    using namespace ImGui;

    static size_t prev_size = 0;
    bool scroll = false;
    {
        size_t t = arduboy->cpu.serial_bytes.size();
        scroll = (t > prev_size);
        prev_size = t;
    }

    if(!open) return;

    SetNextWindowSize({ 400, 200 }, ImGuiCond_FirstUseEver);
    if(Begin("Serial Monitor", &open) && arduboy->cpu.decoded)
    {
        auto& buf = arduboy->cpu.serial_bytes;
        if(Button("Clear"))
            buf.clear();
        auto size = GetContentRegionAvail();
        buf.push_back('\0');
        InputTextMultiline(
            "##serialbuffer",
            (char*)buf.data(),
            buf.size(),
            size,
            ImGuiInputTextFlags_ReadOnly);
        buf.pop_back();
        if(scroll)
        {
            BeginChild("##serialbuffer");
            SetScrollY(GetScrollMaxY());
            EndChild();
        }
    }
    End();
}
