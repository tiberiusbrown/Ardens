#include "common.hpp"
#include "imgui.h"

void view_player()
{
    if(!arduboy->cpu.decoded)
        return;

    auto* d = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
    auto size = ImGui::GetMainViewport()->Size;

    int z = display_filter_zoom();
    float tw = 128 * z;
    float th = 64 * z;
    float w = tw, h = th;
    bool smaller = false;

    while(w > size.x || h > size.y)
        w *= 0.5f, h *= 0.5f, smaller = true;

    while(w + tw < size.x && h + th < size.y)
        w += tw, h += th;
    ImVec2 dsize = { w, h };

    ImVec2 dstart = {
        std::round((size.x - dsize.x) * 0.5f),
        std::round((size.y - dsize.y) * 0.5f)
    };
    d->AddImage(
        display_texture,
        dstart,
        { dstart.x + dsize.x, dstart.y + dsize.y });
}
