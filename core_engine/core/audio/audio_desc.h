#pragma once
#include "core/audio/audio_sample_format.h"


namespace CoreEngine {
    struct AudioDesc {
        int sample_rate = 48000;
        int channel_count = 2;
        AudioSampleFormat sample_format = AudioSampleFormat::Float32;\
        bool start_paused = false;
    };
}
