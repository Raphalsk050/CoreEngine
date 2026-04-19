#pragma once

#include <span>
#include <string_view>

#include "audio_desc.h"

namespace CoreEngine {
    class IAudioBackend {
    public:
        virtual ~IAudioBackend() = default;

        [[nodiscard]] virtual bool Initialize(const AudioDesc &desc) = 0;

        virtual void Shutdown() = 0;

        [[nodiscard]] virtual bool Resume() = 0;

        [[nodiscard]] virtual bool Pause() = 0;

        [[nodiscard]] virtual bool QueueInterleavedFloat32(std::span<const float> samples) = 0;

        [[nodiscard]] virtual int QueuedBytes() const = 0;

        [[nodiscard]] virtual std::string_view LastError() const = 0;
    };
}
