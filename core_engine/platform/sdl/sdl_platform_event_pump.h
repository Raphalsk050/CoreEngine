#pragma once

namespace CoreEngine {
    class SdlInputBackend;
    class SdlWindowBackend;
    class WindowSystem;

    class SdlPlatformEventPump final {
    public:
        SdlPlatformEventPump(SdlWindowBackend &window_backend, SdlInputBackend &input_backend);

        void PumpEvents(WindowSystem &window_system) noexcept;

    private:
        SdlWindowBackend &window_backend_;
        SdlInputBackend &input_backend_;
    };
} // namespace CoreEngine
