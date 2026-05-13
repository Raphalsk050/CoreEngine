#pragma once

#include <memory>

#include "core/platform/i_platform_services.h"
#include "platform/sdl/sdl_context.h"

namespace CoreEngine {
    class SdlInputBackend;
    class SdlPlatformEventPump;
    class SdlWindowBackend;

    /**
     * @brief Adapts SDL backends to the runtime platform service interface.
     *
     * Responsibility: own SDL-only event/input helpers while exposing only
     * engine core systems to Runtime.
     */
    class SdlPlatformServices final : public IPlatformServices {
    public:
        SdlPlatformServices();

        ~SdlPlatformServices() override;

        [[nodiscard]] std::unique_ptr<WindowSystem> CreateWindowSystem() override;

        [[nodiscard]] std::unique_ptr<InputSystem> CreateInputSystem() override;

        [[nodiscard]] std::unique_ptr<AudioSystem> CreateAudioSystem() override;

        void PumpEvents(WindowSystem &window_system) noexcept override;

        void ReleaseInputResources() noexcept override;

        void Shutdown() noexcept override;

    private:
        SdlContext sdl_context_;
        std::unique_ptr<SdlInputBackend> input_backend_;
        std::unique_ptr<SdlPlatformEventPump> event_pump_;
        SdlWindowBackend *window_backend_ = nullptr;
    };
} // namespace CoreEngine
