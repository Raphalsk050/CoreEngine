#pragma once
#include <string>

#include "sdl_context.h"
#include "core/audio/i_audio_backend.h"
#include "SDL3/SDL_audio.h"

namespace CoreEngine {
    class SdlAudioBackend final : public IAudioBackend {
    public:
        explicit SdlAudioBackend(SdlContext &context);

        [[nodiscard]] bool Initialize(const AudioDesc &desc) override;

        void Shutdown() override;

        [[nodiscard]] bool Resume() override;

        [[nodiscard]] bool Pause() override;

        [[nodiscard]] bool QueueInterleavedFloat32(std::span<const float> samples) override;

        [[nodiscard]] int QueuedBytes() const override;

        [[nodiscard]] std::string_view LastError() const override;

    private:
        SdlContext &context_;
        SDL_AudioStream *stream_ = nullptr;
        std::string last_error_;
    };
} // CoreEngine
