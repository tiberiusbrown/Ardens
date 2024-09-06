#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "common.hpp"

#define __USE_SQUARE_BRACKETS_FOR_ELEMENT_ACCESS_OPERATOR
#include <simple_fft/fft.hpp>

static std::array<float, FFT_NUM_SAMPLES / 2> spectrum_data;
static std::array<std::complex<float>, FFT_NUM_SAMPLES> fft_output;

constexpr size_t SAMPLE_HISTORY_MS = 200;
constexpr size_t SAMPLE_HISTORY_NUM = AUDIO_FREQ * SAMPLE_HISTORY_MS / 1000;
static std::vector<float> sample_history;
static size_t sample_history_rem;

void process_sound_samples()
{
    auto const& buffer = arduboy.cpu.sound_buffer;
    size_t index = 0;

    if(buffer.size() >= SAMPLE_HISTORY_NUM)
    {
        sample_history.clear();
        index = buffer.size() - SAMPLE_HISTORY_NUM;
    }
    else if(buffer.size() + sample_history.size() > SAMPLE_HISTORY_NUM)
    {
        size_t n = buffer.size() + sample_history.size() - SAMPLE_HISTORY_NUM;
        sample_history.erase(
            sample_history.begin(),
            sample_history.begin() + n);
    }
    
    while(index < buffer.size() && sample_history.size() < SAMPLE_HISTORY_NUM)
        sample_history.push_back((float)buffer[index++] * (1.f / 32768));

    if(sample_history.size() >= FFT_NUM_SAMPLES)
    {
        char const* error;
        simple_fft::FFT(sample_history.end() - FFT_NUM_SAMPLES, fft_output, FFT_NUM_SAMPLES, error);
        for(size_t i = 0; i < FFT_NUM_SAMPLES / 2; ++i)
            spectrum_data[i] = std::abs(fft_output[i]);
    }

}

static int spectrum_xaxis_formatter(double value, char* buf, int size, void* user)
{
    (void)user;
    double x = value * (1.0 / 1000);
    return snprintf(buf, (size_t)size, "%g", x);
}

static int waveform_xaxis_formatter(double value, char* buf, int size, void* user)
{
    (void)user;
    constexpr double F = 1000.0 / double(AUDIO_FREQ);
    return snprintf(buf, (size_t)size, "%g", SAMPLE_HISTORY_MS - value * F);
}

void window_sound(bool& open)
{
    using namespace ImGui;
    using namespace ImPlot;

    if(!open) return;

    constexpr auto plot_flags =
        ImPlotFlags_NoLegend |
        ImPlotFlags_NoMenus |
        ImPlotFlags_NoBoxSelect |
        0;
    constexpr auto axis_flags =
        ImPlotAxisFlags_NoInitialFit |
        ImPlotAxisFlags_NoMenus |
        ImPlotAxisFlags_NoSideSwitch |
        ImPlotAxisFlags_NoHighlight |
        0;

    SetNextWindowSize({ 400 * pixel_ratio, 400 * pixel_ratio }, ImGuiCond_FirstUseEver);
    if(Begin("Sound", &open) && arduboy.cpu.decoded && arduboy.is_present_state())
    {
        float plot_height = (GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y) * 0.5f;

        if(BeginPlot("Waveform", { -1, plot_height }, plot_flags))
        {
            SetupAxis(ImAxis_X1, "Time (ms)", axis_flags);
            SetupAxis(ImAxis_Y1, nullptr, axis_flags |
                ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels);
            SetupAxisLimits(ImAxis_X1, 0.0, SAMPLE_HISTORY_NUM);
            constexpr double L = double(absim::atmega32u4_t::SOUND_GAIN) * 1.1 / 32768;
            SetupAxisLimits(ImAxis_Y1, -L, L, ImPlotCond_Always);
            SetupAxisLimitsConstraints(ImAxis_X1, 0.0, SAMPLE_HISTORY_NUM);
            SetupAxisFormat(ImAxis_X1, waveform_xaxis_formatter);

            if(!sample_history.empty())
            {
                PlotLine("##wavedata",
                    sample_history.data(),
                    (int)sample_history.size());
            }

            EndPlot();
        }

        if(BeginPlot("Spectrum", { -1, plot_height }, plot_flags))
        {
            SetupAxis(ImAxis_X1, "Frequency (kHz)", axis_flags);
            SetupAxis(ImAxis_Y1, nullptr, axis_flags |
                ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels);

            static constexpr double F = double(AUDIO_FREQ) / FFT_NUM_SAMPLES;
            SetupAxisLimits(ImAxis_X1, 0.0, 5000.0);
            SetupAxisLimits(ImAxis_Y1, 0.0, 30.0, ImPlotCond_Always);
            SetupAxisLimitsConstraints(ImAxis_X1, 0.0, F * spectrum_data.size());
            SetupAxisFormat(ImAxis_X1, spectrum_xaxis_formatter);

            if(!spectrum_data.empty())
            {
                PlotShaded("##specdata",
                    spectrum_data.data(),
                    (int)spectrum_data.size(),
                    0.0, F, 0.0);
            }

            EndPlot();
        }

    }
    End();
}
