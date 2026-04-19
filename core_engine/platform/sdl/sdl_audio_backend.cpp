// platform/sdl/sdl_audio_backend.cpp
#include "platform/sdl/sdl_audio_backend.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>

namespace CoreEngine {
    namespace {
        SDL_AudioFormat ToSdlAudioFormat(AudioSampleFormat format) {
            switch (format) {
                case AudioSampleFormat::Float32:
                    return SDL_AUDIO_F32;
            }

            return SDL_AUDIO_F32;
        }
    }

    SdlAudioBackend::SdlAudioBackend(SdlContext &context)
        : context_(context) {
    }

    bool SdlAudioBackend::Initialize(const AudioDesc &desc) {
        if (!context_.InitializeAudio()) {
            last_error_ = SDL_GetError();
            return false;
        }

        const SDL_AudioSpec spec{
            .format = ToSdlAudioFormat(desc.sample_format),
            .channels = desc.channel_count,
            .freq = desc.sample_rate,
        };

        stream_ = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
            &spec,
            nullptr,
            nullptr
        );

        if (stream_ == nullptr) {
            last_error_ = SDL_GetError();
            return false;
        }

        if (!desc.start_paused && !Resume()) {
            return false;
        }

        return true;
    }

    void SdlAudioBackend::Shutdown() {
        if (stream_ != nullptr) {
            SDL_DestroyAudioStream(stream_);
            stream_ = nullptr;
        }
    }

    bool SdlAudioBackend::Resume() {
        if (stream_ == nullptr) {
            last_error_ = "Audio stream is not initialized";
            return false;
        }

        if (!SDL_ResumeAudioStreamDevice(stream_)) {
            last_error_ = SDL_GetError();
            return false;
        }

        return true;
    }

    bool SdlAudioBackend::Pause() {
        if (stream_ == nullptr) {
            last_error_ = "Audio stream is not initialized";
            return false;
        }

        if (!SDL_PauseAudioStreamDevice(stream_)) {
            last_error_ = SDL_GetError();
            return false;
        }

        return true;
    }

    bool SdlAudioBackend::QueueInterleavedFloat32(std::span<const float> samples) {
        if (stream_ == nullptr) {
            last_error_ = "Audio stream is not initialized";
            return false;
        }

        const auto byteCount = static_cast<int>(samples.size_bytes());

        if (!SDL_PutAudioStreamData(stream_, samples.data(), byteCount)) {
            last_error_ = SDL_GetError();
            return false;
        }

        return true;
    }

    int SdlAudioBackend::QueuedBytes() const {
        if (stream_ == nullptr) {
            return 0;
        }

        return SDL_GetAudioStreamQueued(stream_);
    }

    std::string_view SdlAudioBackend::LastError() const {
        return last_error_;
    }
} // namespace CoreEngine
