#include "imgui.h"
#include "imgui_memory_editor.h"

#include "absim.hpp"

extern std::unique_ptr<absim::arduboy_t> arduboy;
extern int fx_data_scroll_addr;

static MemoryEditor memed_fx;
static char minpagebuf[5];
static char maxpagebuf[5];

static uint32_t minpage;
static uint32_t maxpage;
static bool autopage = true;

static int hex_value(char c)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static int hex_value(char const* c)
{
    int r = 0;
    while(*c != 0)
        r = (r << 4) + hex_value(*c++);
    return r;
}

static bool highlight_func(ImU8 const* data, size_t off, ImU32& color)
{
    bool r = false;
    if(off + arduboy->fx.min_page * 256 == arduboy->fx.current_addr)
    {
        if(arduboy->fx.reading || arduboy->fx.programming)
        {
            color = IM_COL32(40, 160, 40, 255);
            r = true;
        }
    }
    return r;
}

void window_fx_data(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 400, 400 }, ImGuiCond_FirstUseEver);
    if(Begin("FX Data", &open))
    {
        if(autopage || !*minpagebuf)
            snprintf(minpagebuf, sizeof(minpagebuf), "%04x", arduboy->fx.min_page);
        if(autopage || !*maxpagebuf)
            snprintf(maxpagebuf, sizeof(maxpagebuf), "%04x", arduboy->fx.max_page);

        float tw = CalcTextSize("FFFF").x + GetStyle().FramePadding.x * 2;
        AlignTextToFramePadding();
        TextUnformatted("Page Range: 0x");
        SameLine(0.f, 0.f);
        SetNextItemWidth(tw);
        BeginDisabled(autopage);
        if(InputText("##minpage", minpagebuf, sizeof(minpagebuf),
            ImGuiInputTextFlags_CharsHexadecimal |
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll))
        {
            minpage = (uint32_t)hex_value(minpagebuf);
        }
        EndDisabled();
        SameLine(0.f, 0.f);
        TextUnformatted(" to 0x");
        SameLine(0.f, 0.f);
        SetNextItemWidth(tw);
        BeginDisabled(autopage);
        if(InputText("##maxpage", maxpagebuf, sizeof(maxpagebuf),
            ImGuiInputTextFlags_CharsHexadecimal |
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll))
        {
            maxpage = (uint32_t)hex_value(maxpagebuf);
        }
        EndDisabled();
        SameLine();
        Checkbox("Auto", &autopage);
        if(autopage)
        {
            minpage = arduboy->fx.min_page;
            maxpage = arduboy->fx.max_page;
        }

        maxpage = std::min<uint32_t>(maxpage, 0xffff);
        minpage = std::min<uint32_t>(minpage, maxpage);
        memed_fx.OptAddrDigitsCount = 6;
        if(fx_data_scroll_addr >= 0)
            memed_fx.GotoAddr = (size_t)(fx_data_scroll_addr - minpage * 256);
        fx_data_scroll_addr = -1;
        memed_fx.HighlightFn = highlight_func;
        memed_fx.DrawContents(
            arduboy->fx.data.data() + minpage * 256,
            size_t((maxpage - minpage + 1) * 256),
            minpage * 256);
    }
    End();
}
