#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "common.hpp"

#include "AudioFile/AudioFile.h"

bool wav_recording = false;
static char wav_fname[256];

static AudioFile<int16_t> af;

void send_wav_audio()
{
    if(!wav_recording) return;
    af.samples[0].insert(
        af.samples[0].end(),
        arduboy->cpu.sound_buffer.begin(),
        arduboy->cpu.sound_buffer.end());
}

void wav_recording_toggle()
{
    std::vector<std::vector<int16_t>> blank(1);
    if(wav_recording)
    {
        send_wav_audio();
#ifdef __EMSCRIPTEN__
        af.save("recording.wav");
        file_download("recording.wav", wav_fname, "audio/x-wav");
#else
        af.save(wav_fname);
#endif
        af.samples.swap(blank);
    }
    else
    {
        time_t rawtime;
        struct tm* ti;
        time(&rawtime);
        ti = localtime(&rawtime);
        (void)snprintf(wav_fname, sizeof(wav_fname),
            "recording_%04d%02d%02d%02d%02d%02d.wav",
            ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
            ti->tm_hour + 1, ti->tm_min, ti->tm_sec);
        af.samples.swap(blank);
        af.setBitDepth(16);
        af.setSampleRate(AUDIO_FREQ);
        send_wav_audio();
    }
    wav_recording = !wav_recording;
}
