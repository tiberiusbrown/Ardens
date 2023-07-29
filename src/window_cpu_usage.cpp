#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "common.hpp"

#include <numeric>

extern std::unique_ptr<absim::arduboy_t> arduboy;

static int yaxis_formatter(double value, char* buf, int size, void* user)
{
    (void)user;
    return snprintf(buf, (size_t)size, "%.0f%%", value * 100);
}

static void window_contents()
{
    using namespace ImPlot;

    if(arduboy->frame_cpu_usage.empty())
        return;

    {
        float usage = 0.f;
        size_t i;
        for(i = 0; i < 16; ++i)
        {
            if(i >= arduboy->frame_cpu_usage.size())
                break;
            usage += arduboy->frame_cpu_usage[arduboy->frame_cpu_usage.size() - i - 1];
        }
        usage /= i;
        bool red = arduboy->frame_cpu_usage.back() > 0.999;
        if(red) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        ImGui::Text("CPU Usage: %5.1f%%", usage * 100);
        if(red) ImGui::PopStyleColor();
    }

    if(arduboy->prev_frame_cycles != 0)
    {
        static std::array<float, 16> fps_queue{};
        static size_t fps_i = 0;
        float fps = 16e6f / float(arduboy->prev_frame_cycles);
        fps_queue[fps_i] = fps;
        fps_i = (fps_i + 1) % fps_queue.size();
        fps = std::accumulate(fps_queue.begin(), fps_queue.end(), 0.f) * (1.f / fps_queue.size());
        ImGui::SameLine();
        ImGui::Text("     FPS: %5.1f", fps);
    }

    constexpr auto plot_flags =
        ImPlotFlags_NoTitle |
        ImPlotFlags_NoLegend |
        ImPlotFlags_NoMenus |
        ImPlotFlags_NoBoxSelect |
        0;

    if(BeginPlot("##cpu_usage", { -1, -1 }, plot_flags))
    {
        auto* plot = GetCurrentPlot();
        constexpr double ZOOM = 4.0;
        constexpr double IZOOM = 1.0 / ZOOM;
        double n = double(arduboy->total_frames);
        double m = n - arduboy->frame_cpu_usage.size();
        double w = plot->FrameRect.GetWidth();

        double z = plot->Axes[ImAxis_X1].Range.Size();

        constexpr auto axis_flags =
            ImPlotAxisFlags_NoInitialFit |
            ImPlotAxisFlags_NoMenus |
            ImPlotAxisFlags_NoSideSwitch |
            ImPlotAxisFlags_NoHighlight |
            //ImPlotAxisFlags_NoDecorations |
            0;
        SetupAxis(ImAxis_X1, "Frame", axis_flags);
        SetupAxis(ImAxis_Y1, nullptr, axis_flags);
        SetupAxisFormat(ImAxis_Y1, yaxis_formatter);

        double lim_min = m;
        double lim_max = (n < z ? z : n);
        if(!arduboy->paused)
        {
            if(n < w)
                lim_max = w;
            else
            {
                lim_min = n - w;
                lim_max = n;
            }
            SetupAxisLimitsConstraints(ImAxis_X1, lim_min, lim_max);
            SetupAxisZoomConstraints(ImAxis_X1, w, w);
            plot->Axes[ImAxis_X1].Range = { lim_min, lim_max };
        }
        else
        {
            SetupAxisLimitsConstraints(ImAxis_X1, lim_min, lim_max);
            SetupAxisZoomConstraints(ImAxis_X1, w / 16, w);
        }

        SetupAxisLimits(ImAxis_Y1, 0.0, 1.0, ImPlotCond_Always);
        SetupFinish();
        PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
        PlotShaded("##usage",
            arduboy->frame_cpu_usage.data(),
            (int)arduboy->frame_cpu_usage.size(),
            0.0, 1.0, m);
        PopStyleVar();
        PlotLine("##usage",
            arduboy->frame_cpu_usage.data(),
            (int)arduboy->frame_cpu_usage.size(),
            1.0, m);
        EndPlot();
    }
}

void window_cpu_usage(bool& open)
{
    if(!open) return;

    ImGui::SetNextWindowSize({ 400 * pixel_ratio, 200 * pixel_ratio }, ImGuiCond_FirstUseEver);
    if(ImGui::Begin("CPU Usage", &open) && arduboy->cpu.decoded)
    {
        window_contents();
    }
    ImGui::End();
    //ImPlot::ShowDemoWindow();
}
