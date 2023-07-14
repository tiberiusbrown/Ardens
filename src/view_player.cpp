#include "common.hpp"
#include "imgui.h"

#include <algorithm>

void display_with_scanlines(ImDrawList* d, ImVec2 const& a, ImVec2 const& b)
{
    {
        std::array<ImVec2, 4> vs;
        vs[0] = {a.x, a.y};
        vs[1] = {b.x, a.y};
        vs[2] = {b.x, b.y};
        vs[3] = {a.x, b.y};
        std::rotate(vs.begin(), vs.begin() + settings.display_orientation, vs.end());
        d->AddImageQuad(
            display_texture,
            vs[0], vs[1], vs[2], vs[3]);
    }

    if(settings.display_pixel_grid == PGRID_NONE) return;
    if(display_texture_zoom != 1) return;
    float w = b.x - a.x;

    int numh = 128;
    int numv = 64;
    if(settings.display_orientation & 1)
        std::swap(numh, numv);

    if(w < numh * 3) return;

    ImU32 line_color;
    constexpr uint8_t T = 192;
    constexpr uint8_t A = 128;
    switch(settings.display_pixel_grid)
    {
    case PGRID_RED:     line_color = IM_COL32(T, 0, 0, A); break;
    case PGRID_GREEN:   line_color = IM_COL32(0, T, 0, A); break;
    case PGRID_BLUE:    line_color = IM_COL32(0, 0, T, A); break;
    case PGRID_CYAN:    line_color = IM_COL32(0, T, T, A); break;
    case PGRID_MAGENTA: line_color = IM_COL32(T, 0, T, A); break;
    case PGRID_YELLOW:  line_color = IM_COL32(T, T, 0, A); break;
    case PGRID_WHITE:   line_color = IM_COL32(T, T, T, A); break;
    case PGRID_NORMAL:
    default:
    {
        uint8_t t[4];
        palette_rgba(settings.display_palette, 0, t);
        line_color = IM_COL32(t[0], t[1], t[2], A);
        break;
    }
    }

    float const inc = w / numh;
    float const line_thickness = inc * 0.125f;
    float const off = line_thickness * 0.5f;

    // draw pixel grid
    for(int i = 0; i <= numh; ++i)
    {
        ImVec2 p1 = { a.x + inc * i - off, a.y };
        ImVec2 p2 = { a.x + inc * i + off, b.y };
        d->PathRect(p1, p2);
        d->PathFillConvex(line_color);
    }
    for(int i = 0; i <= numv; ++i)
    {
        ImVec2 p1 = { a.x, a.y + inc * i - off };
        ImVec2 p2 = { b.x, a.y + inc * i + off };
        d->PathRect(p1, p2);
        d->PathFillConvex(line_color);
    }
}

void view_player()
{
    if(!arduboy->cpu.decoded)
        return;

    auto* d = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
    auto size = ImGui::GetMainViewport()->Size;

    int z = display_filter_zoom();
    float tw = 128 * z;
    float th = 64 * z;
    if(settings.display_orientation & 1)
        std::swap(tw, th);
    float w = tw, h = th;
    bool smaller = false;

    while(w > size.x || h > size.y)
        w *= 0.5f, h *= 0.5f, smaller = true;

    while(w + tw <= size.x && h + th <= size.y)
        w += tw, h += th;
    ImVec2 dsize = { w, h };

    ImVec2 dstart = {
        std::round((size.x - dsize.x) * 0.5f),
        std::round((size.y - dsize.y) * 0.5f)
    };
    display_with_scanlines(d, dstart,
        { dstart.x + dsize.x, dstart.y + dsize.y });

    if(gif_recording)
    {
        float const F1 = 10.f * pixel_ratio;
        float const F2 = 20.f * pixel_ratio;
        d->AddRectFilled(
            { F1, F1 }, { F2, F2 },
            IM_COL32(255, 0, 0, 128));
    }
}
