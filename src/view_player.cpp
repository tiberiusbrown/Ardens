#include "common.hpp"
#include "imgui.h"

#include <algorithm>

#include <cmath>

// as a factor of width
constexpr float ARROW_SIZE = 3.f / 32;
constexpr float AB_SIZE = 4.f / 32;

static ImVec2 rect_viewsize()
{
    auto size = ImGui::GetMainViewport()->Size;
    float w = std::min(size.x, size.y * 2.f);
    float h = w * 0.5f;
    return { w, h };
}

static ImVec2 rect_offset()
{
    auto size = ImGui::GetMainViewport()->Size;
    auto rsize = rect_viewsize();
    float x = (size.x - rsize.x) * 0.5f;
    float y = (size.y - rsize.y) * 0.5f;
    y = std::clamp(y + rsize.y * 0.5f, y, size.y - rsize.y);
    return { x, y };
}

touch_rect_t touch_rect(int btn)
{
    if((unsigned)btn >= 6) return {};
    constexpr float OX[6] = { 6, 6, 2, 10, 18, 25 };
    constexpr float OY[6] = { 2, 10, 6, 6, 7, 4 };
    constexpr float SZ[6] = { 4, 4, 4, 4, 5, 5 };
    touch_rect_t r{};
    auto off = rect_offset();
    auto rsize = rect_viewsize();
    float size = rsize.x * SZ[btn] / 32.f;
    
    //float btnsize = rsize.x * pixel_ratio * (3.f / 32.f);
    //float scale = std::clamp(50.f / btnsize, 1.f, 1.4f);
    constexpr float scale = 1.f;

    r.x0 = off.x + rsize.x * scale * (OX[btn] / 32.f);
    r.y0 = off.y + rsize.y * scale * (OY[btn] / 16.f);
    //if(btn == TOUCH_A || btn == TOUCH_B)
    //    r.x0 = rsize.x - r.x0 - size * scale;
    r.x1 = r.x0 + size * scale;
    r.y1 = r.y0 + size * scale;
    return r;
}

static void draw_button(ImDrawList* d, int btn, touched_buttons_t const& pressed)
{
    constexpr ImU32 pcol = IM_COL32(150, 150, 150, 150);
    constexpr ImU32 col = IM_COL32(100, 100, 100, 70);
    constexpr ImU32 outline_col = IM_COL32(50, 50, 50, 150);
    constexpr float ROT[4] = { 270, 90, 180, 0 };
    float thickness = pixel_ratio * 3.f;
    auto r = touch_rect(btn);

    switch(btn)
    {
    case TOUCH_U:
    case TOUCH_D:
    case TOUCH_L:
    case TOUCH_R:
    {
        // bevel
        constexpr float B = 0.075f;
        constexpr ImVec2 DPAD[] =
        {
            { -1.f, +0.f + B },
            { -1.f, +0.f - B },
            { +0.f - B, -1.f },
            { +1.f - B, -1.f },
            { +1.f, -1.f + B },
            { +1.f, +1.f - B },
            { +1.f - B, +1.f },
            { +0.f - B, +1.f },
        };
        constexpr int N_DPAD = sizeof(DPAD) / sizeof(DPAD[0]);
        float rot = ROT[btn] * (3.1415926535f / 180);
        float tc = cosf(rot);
        float ts = sinf(rot);
        float scale = (r.x1 - r.x0) * 0.72f;
        r.x0 = (r.x0 + r.x1) * 0.5f;
        r.y0 = (r.y0 + r.y1) * 0.5f;
        ImVec2 dp[N_DPAD + 2];
        for(int i = 0; i < N_DPAD; ++i)
        {
            dp[i].x = (tc * DPAD[i].x - ts * DPAD[i].y) * scale + r.x0;
            dp[i].y = (ts * DPAD[i].x + tc * DPAD[i].y) * scale + r.y0;
        }
        dp[N_DPAD + 0] = dp[0];
        dp[N_DPAD + 1] = dp[1];
        d->AddConvexPolyFilled(dp, N_DPAD, pressed.btns[btn] ? pcol : col);
        d->AddPolyline(dp, N_DPAD, outline_col, ImDrawFlags_Closed, thickness);
        break;
    }
    case TOUCH_A:
    case TOUCH_B:
    {
        constexpr int SEGMENTS = 32;
        float radius = (r.x1 - r.x0) * 0.65f;
        r.x0 = (r.x0 + r.x1) * 0.5f;
        r.y0 = (r.y0 + r.y1) * 0.5f;
        d->AddCircleFilled(
            { r.x0, r.y0 }, radius,
            pressed.btns[btn] ? pcol : col,
            SEGMENTS);
        d->AddCircle(
            { r.x0, r.y0 }, radius,
            outline_col,
            SEGMENTS,
            thickness);
        break;
    }
    default:
        break;
    }
}

touched_buttons_t touched_buttons()
{
    touched_buttons_t t{};
    for(int i = 0; i < 6; ++i)
    {
        auto r = touch_rect(i);
        float s = 0.3f * (r.x1 - r.x0);
        for(auto const& [k, v] : touch_points)
        {
            if(!(v.x >= r.x0 - s && v.x < r.x1 + s)) continue;
            if(!(v.y >= r.y0 - s && v.y < r.y1 + s)) continue;
            t.btns[i] = true;
        }
    }
    return t;
}

void display_with_scanlines(ImDrawList* d, ImVec2 const& a, ImVec2 const& b)
{
    platform_texture_scale_nearest(display_texture);

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
    float const line_thickness = inc * (1.f / 16);
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
    if(!arduboy.cpu.decoded)
        return;

    auto* d = ImGui::GetBackgroundDrawList();
    auto size = ImGui::GetMainViewport()->Size;

    int z = display_filter_zoom();
    float tw = 128.f * z;
    float th = 64.f * z;
    if(settings.display_orientation & 1)
        std::swap(tw, th);
    float w = tw, h = th;
    bool smaller = false;

    if(settings.display_integer_scale)
    {
        while(w > size.x || h > size.y)
            w *= 0.5f, h *= 0.5f, smaller = true;
        while(w + tw <= size.x && h + th <= size.y)
            w += tw, h += th;
    }
    else if(settings.display_orientation & 1)
    {
        w = size.x;
        h = w * 2.f;
        if(h > size.y)
        {
            h = size.y;
            w = h * 0.5f;
        }
    }
    else
    {
        w = size.x;
        h = w * 0.5f;
        if(h > size.y)
        {
            h = size.y;
            w = h * 2.f;
        }
    }
    ImVec2 dsize = { w, h };

    ImVec2 dstart = {
        std::round((size.x - dsize.x) * 0.5f),
        std::round((size.y - dsize.y) * 0.5f)
    };
    if(first_touch || always_touch)
        dstart.y = std::max(0.f, dstart.y - dsize.y * 0.5f);
    dstart.x = std::round(dstart.x);
    dstart.y = std::round(dstart.y);
    display_with_scanlines(d, dstart,
        { dstart.x + dsize.x, dstart.y + dsize.y });

    // draw touch icons
    if(always_touch || first_touch && (
        ms_since_touch < MS_SHOW_TOUCH_CONTROLS || rect_offset().y >= dstart.y + dsize.y))
    {
        auto pressed = touched_buttons();
        draw_button(d, TOUCH_U, pressed);
        draw_button(d, TOUCH_D, pressed);
        draw_button(d, TOUCH_L, pressed);
        draw_button(d, TOUCH_R, pressed);
        draw_button(d, TOUCH_A, pressed);
        draw_button(d, TOUCH_B, pressed);
    }

    if(gif_recording)
    {
        float const F1 = 10.f * pixel_ratio;
        float const F2 = 20.f * pixel_ratio;
        d->AddRectFilled(
            { F1, F1 }, { F2, F2 },
            IM_COL32(255, 0, 0, 128));
    }

#if 0
    // debug viewport border
    d->AddRect({ 0, 0 }, size, IM_COL32(255, 0, 0, 255), 0, 0, 4);
#endif
}
