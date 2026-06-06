#pragma once
#include <memory>

#include "core/audio/i_audio_backend.h"

namespace CoreEngine {
    class AudioSystem final {
    public:
        explicit AudioSystem(std::unique_ptr<IAudioBackend> backend);

        [[nodiscard]] bool Initialize(const AudioDesc &desc) const;

        void Shutdown() const;

        [[nodiscard]] bool Resume() const;

        [[nodiscard]] bool Pause() const;

        [[nodiscard]] bool QueueInterleavedFloat32(std::span<const float> samples) const;

        [[nodiscard]] int QueuedBytes() const;

        [[nodiscard]] std::string_view LastError() const;

    private:
        std::unique_ptr<IAudioBackend> backend_;
    };
} // namespace CoreEngine
