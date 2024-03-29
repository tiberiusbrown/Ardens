#include "common.hpp"

#include <string>

#include <imgui.h>
#include "imgui_markdown/imgui_markdown.h"

static char const MARKDOWN[] = R"(*Ardens*
***

[Arduboy](https://www.arduboy.com/) simulator for profiling and debugging ([source Code](https://github.com/tiberiusbrown/Ardens)).

Drap and drop an [Arduboy game](https://community.arduboy.com/c/games/35) (hex/elf/arduboy) and any FX data (bin) onto this window.

*Libraries Used*
***
  * [sokol](https://github.com/floooh/sokol) (zlib)
  * [LLVM Project](https://github.com/llvm/llvm-project) (Apache v2.0 with LLVM Exceptions) 
  * [Dear ImGui](https://github.com/ocornut/imgui) (MIT)
  * [ImPlot](https://github.com/epezent/implot) (MIT)
  * [imgui_markdown](https://github.com/juliettef/imgui_markdown) (zlib)
  * [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) (MIT)
  * [Simple-FFT](https://github.com/d1vanov/Simple-FFT) (MIT)
  * [brunexgeek/hqx](https://github.com/brunexgeek/hqx) (Apache v2.0)
  * [{fmt}](https://github.com/fmtlib/fmt) (MIT)
  * [rapidjson](https://github.com/Tencent/rapidjson) (MIT)
  * [Miniz](https://github.com/richgel999/miniz) (MIT)
  * [Bitsery](https://github.com/fraillt/bitsery) (MIT)
  * [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h) (Public Domain)
  * [GIF encoder](https://github.com/lecram/gifenc) (Public Domain)
  * [AudioFile](https://github.com/adamstark/AudioFile) (MIT)
  * [Emscripten Browser File Library](https://github.com/Armchair-Software/emscripten-browser-file) (MIT)

*License*
***

MIT License

Copyright (c) 2023 Peter Brown

)"
"Permission is hereby granted, free of charge, to any person obtaining a copy "
"of this software and associated documentation files (the \"Software\"), to deal "
"in the Software without restriction, including without limitation the rights "
"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
"copies of the Software, and to permit persons to whom the Software is "
"furnished to do so, subject to the following conditions:\n"
"\n"
"The above copyright notice and this permission notice shall be included in all "
"copies or substantial portions of the Software.\n"
"\n"
"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
"SOFTWARE.\n"
;

static void link_callback(ImGui::MarkdownLinkCallbackData data)
{
    if(data.isImage) return;
    std::string url(data.link, data.linkLength);
    platform_open_url(url.c_str());
}

static void tooltip_callback(ImGui::MarkdownTooltipCallbackData data)
{
    using namespace ImGui;
    SetMouseCursor(ImGuiMouseCursor_Hand);
    BeginTooltip();
    TextUnformatted(data.linkData.link, data.linkData.link + data.linkData.linkLength);
    EndTooltip();
}

void modal_about()
{
    using namespace ImGui;

    SetNextWindowSize({ pixel_ratio * 500.f, pixel_ratio * 400.f });
    if(!BeginPopupModal("About", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings))
        return;

    float ch = GetContentRegionAvail().y;
    ch -= GetFrameHeightWithSpacing();
    ch -= GetFrameHeight();
    if(BeginChild("##content", { 0.f, ch }, true))
    {

        MarkdownConfig config{};
        config.linkCallback = link_callback;
        config.tooltipCallback = tooltip_callback;
        Markdown(MARKDOWN, sizeof(MARKDOWN) - 1, config);
    }
    EndChild();

    SetCursorPosY(GetCursorPosY() + GetFrameHeight());

    if(Button("OK", ImVec2(120 * pixel_ratio, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter))
        CloseCurrentPopup();

    EndPopup();
}
