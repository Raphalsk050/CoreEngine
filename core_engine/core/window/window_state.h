#pragma once

namespace CoreEngine {
    struct WindowExtent {
        int width = 0;
        int height = 0;

        [[nodiscard]] bool IsValid() const noexcept {
            return width > 0 && height > 0;
        }
    };

    struct WindowState {
        WindowExtent logical_size{};
        WindowExtent pixel_size{};
        bool focused = true;
        bool minimized = false;

        [[nodiscard]] bool IsRenderable() const noexcept {
            return pixel_size.width > 0 && pixel_size.height > 0 && !minimized;
        }
    };
} // namespace CoreEngine
