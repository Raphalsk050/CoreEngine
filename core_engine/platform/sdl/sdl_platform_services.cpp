#include "platform/sdl/sdl_platform_services.h"

#include "core/audio/audio_system.h"
#include "core/input/input_system.h"
#include "core/window/window_system.h"
#include "platform/sdl/sdl_audio_backend.h"
#include "platform/sdl/sdl_input_backend.h"
#include "platform/sdl/sdl_platform_event_pump.h"
#include "platform/sdl/sdl_window_backend.h"

namespace CoreEngine {
    SdlPlatformServices::SdlPlatformServices() = default;

    SdlPlatformServices::~SdlPlatformServices() = default;

    std::unique_ptr<WindowSystem> SdlPlatformServices::CreateWindowSystem() {
        auto backend = std::make_unique<SdlWindowBackend>(sdl_context_);
        window_backend_ = backend.get();
        return std::make_unique<WindowSystem>(std::move(backend));
    }

    std::unique_ptr<InputSystem> SdlPlatformServices::CreateInputSystem() {
        auto input_system = std::make_unique<InputSystem>();
        input_backend_ = std::make_unique<SdlInputBackend>(*input_system);

        if (window_backend_ != nullptr) {
            event_pump_ = std::make_unique<SdlPlatformEventPump>(*window_backend_, *input_backend_);
        }

        return input_system;
    }

    std::unique_ptr<AudioSystem> SdlPlatformServices::CreateAudioSystem() {
        return std::make_unique<AudioSystem>(std::make_unique<SdlAudioBackend>(sdl_context_));
    }

    void SdlPlatformServices::PumpEvents(WindowSystem &window_system) noexcept {
        if (event_pump_ != nullptr) {
            event_pump_->PumpEvents(window_system);
            return;
        }

        window_system.PollEvents();
    }

    void SdlPlatformServices::ReleaseInputResources() noexcept {
        event_pump_.reset();
        input_backend_.reset();
    }

    void SdlPlatformServices::Shutdown() noexcept {
        ReleaseInputResources();
        window_backend_ = nullptr;
        sdl_context_.Shutdown();
    }
} // namespace CoreEngine
