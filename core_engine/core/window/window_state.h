#pragma once

namespace CoreEngine {
    struct WindowExtent {
        int width = 0;
        int height = 0;

        [[nodiscard]] bool IsValid() const noexcept {
            return width > 0 && height > 0;
        }
    };

    enum class WindowCursorMode : int8_t {
        CURSOR_NORMAL = 0,
        CURSOR_CONSTRAINED_AND_HIDDEN,
        CURSOR_CONSTRAINED,
    };

    struct WindowState {
        WindowExtent logical_size{};
        WindowExtent pixel_size{};
        bool focused = true;
        bool minimized = false;
        WindowCursorMode cursor_mode = WindowCursorMode::CURSOR_NORMAL;

        [[nodiscard]] bool IsRenderable() const noexcept {
            return pixel_size.width > 0 && pixel_size.height > 0 && !minimized;
        }
    };
} // namespace CoreEngine
