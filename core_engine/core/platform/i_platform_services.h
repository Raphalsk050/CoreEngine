#pragma once

#include <memory>

namespace CoreEngine {
    class AudioSystem;
    class InputSystem;
    class WindowSystem;

    /**
     * @brief Owns platform-specific services needed by the runtime shell.
     *
     * Responsibility: create core-facing systems from platform backends and keep
     * native event/input resources behind the platform abstraction boundary.
     */
    class IPlatformServices {
    public:
        virtual ~IPlatformServices() = default;

        [[nodiscard]] virtual std::unique_ptr<WindowSystem> CreateWindowSystem() = 0;

        [[nodiscard]] virtual std::unique_ptr<InputSystem> CreateInputSystem() = 0;

        [[nodiscard]] virtual std::unique_ptr<AudioSystem> CreateAudioSystem() = 0;

        virtual void PumpEvents(WindowSystem &window_system) noexcept = 0;

        virtual void ReleaseInputResources() noexcept = 0;

        virtual void Shutdown() noexcept = 0;
    };
} // namespace CoreEngine
