#include "common.hpp"
#include "imgui.h"

void view_player()
{
    auto* d = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    auto size = ImGui::GetMainViewport()->Size;
    ImVec2 dsize{};
    for(int z = 1;; ++z)
    {
        dsize = { 128.f * z, 64.f * z };
        if(dsize.x + 128 > size.x) break;
        if(dsize.y + 64 > size.y) break;
    }
    ImVec2 dstart = {
        std::round((size.x - dsize.x) * 0.5f),
        std::round((size.y - dsize.y) * 0.5f)
    };
    d->AddImage(
        display_texture,
        dstart,
        { dstart.x + dsize.x, dstart.y + dsize.y });
}
