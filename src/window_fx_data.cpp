#include "imgui.h"
#include "imgui_memory_editor.h"

#include "common.hpp"

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

static ImU32 bgcolor_func(ImU8 const* data, size_t off, void* user)
{
    (void)user;
    if(off + app.emulator->peripherals.fx.min_page * absim::w25q128_t::PAGE_BYTES ==
        app.emulator->peripherals.fx.current_addr)
    {
        if(app.emulator->peripherals.fx.reading || app.emulator->peripherals.fx.programming)
        {
            return IM_COL32(40, 160, 40, 255);
        }
    }
    return 0;
}

void window_fx_data(bool& open)
{
	using namespace ImGui;
    if(!open) return;

    SetNextWindowSize({ 400 * app.pixel_ratio, 400 * app.pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("FX Data", &open))
    {
        if(autopage || !*minpagebuf)
            snprintf(minpagebuf, sizeof(minpagebuf), "%04x", app.emulator->peripherals.fx.min_page);
        if(autopage || !*maxpagebuf)
            snprintf(maxpagebuf, sizeof(maxpagebuf), "%04x", app.emulator->peripherals.fx.max_page);

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
            minpage = app.emulator->peripherals.fx.min_page;
            maxpage = app.emulator->peripherals.fx.max_page;
        }

        SameLine();
        if(Button("Reset all data"))
        {
            app.emulator->peripherals.fx.erase_all_data();
            app.emulator->program_state.fxdata.clear();
            app.emulator->program_state.fxsave.clear();
            app.emulator->update_game_hash();
            app.emulator->reset();
            load_savedata();
        }

        maxpage = std::min<uint32_t>(maxpage, absim::w25q128_t::LAST_PAGE);
        minpage = std::min<uint32_t>(minpage, maxpage);
        memed_fx.OptAddrDigitsCount = 6;
        if(app.fx_data_scroll_addr >= 0)
            memed_fx.GotoAddr = (size_t)(app.fx_data_scroll_addr -
                minpage * absim::w25q128_t::PAGE_BYTES);
        app.fx_data_scroll_addr = -1;
        memed_fx.BgColorFn = bgcolor_func;
        memed_fx.ReadFn = [](ImU8 const* data, size_t off, void* user) {
            (void)data;
            (void)user;
            return app.emulator->peripherals.fx.read_byte(
                off + minpage * absim::w25q128_t::PAGE_BYTES);
        };
        memed_fx.WriteFn = [](ImU8* data, size_t off, ImU8 d, void* user) {
            (void)data;
            (void)user;
            app.emulator->peripherals.fx.write_byte(
                off + minpage * absim::w25q128_t::PAGE_BYTES, d);
        };
        memed_fx.ReadOnly = !app.emulator->is_present_state();
        memed_fx.DrawContents(
            nullptr,
            size_t((maxpage - minpage + 1) * absim::w25q128_t::PAGE_BYTES),
            minpage * absim::w25q128_t::PAGE_BYTES);
    }
    End();
}
