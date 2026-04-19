#include "core/audio/audio_system.h"

namespace CoreEngine {
    AudioSystem::AudioSystem(std::unique_ptr<IAudioBackend> backend)
        : backend_(std::move(backend)) {
    }

    bool AudioSystem::Initialize(const AudioDesc &desc) const {
        return backend_ != nullptr && backend_->Initialize(desc);
    }

    void AudioSystem::Shutdown() const {
        if (backend_ != nullptr) {
            backend_->Shutdown();
        }
    }

    bool AudioSystem::Resume() const {
        return backend_ != nullptr && backend_->Resume();
    }

    bool AudioSystem::Pause() const {
        return backend_ != nullptr && backend_->Pause();
    }

    bool AudioSystem::QueueInterleavedFloat32(const std::span<const float> samples) const {
        return backend_ != nullptr && backend_->QueueInterleavedFloat32(samples);
    }

    int AudioSystem::QueuedBytes() const {
        return backend_ != nullptr ? backend_->QueuedBytes() : 0;
    }

    std::string_view AudioSystem::LastError() const {
        return backend_ != nullptr ? backend_->LastError() : "Audio backend is not available";
    }
} // namespace CoreEngine
