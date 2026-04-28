#pragma once

namespace CoreEngine {
    enum class NativeWindowPlatform {
        Unknown,
        Win32,
        MacOS
    };

    struct NativeWindowHandle {
        NativeWindowPlatform platform = NativeWindowPlatform::Unknown;
        void *window = nullptr;
        void *display = nullptr;
        void *platform_window = nullptr;

        [[nodiscard]] bool IsValid() const {
            return window != nullptr;
        }
    };
} // namespace CoreEngine
