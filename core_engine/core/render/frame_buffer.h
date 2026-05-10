#pragma once

namespace CoreEngine {
    enum class FrameBufferFormat {
        SwapChainColor,
        SwapChainDepth,
        RGBA8Unorm,
        RGBA16Float,
        R32Float,
        Depth32Float,
    };

    struct FrameBufferDesc {
        int width = 0;
        int height = 0;
        bool sample_color = true;
        bool has_color = true;
        bool has_depth = true;
        bool sample_depth = false;
        FrameBufferFormat color_format = FrameBufferFormat::SwapChainColor;
        FrameBufferFormat depth_format = FrameBufferFormat::SwapChainDepth;

        [[nodiscard]] bool IsValid() const {
            return width > 0 &&
                   height > 0 &&
                   (has_color || has_depth) &&
                   (!sample_color || has_color) &&
                   (!sample_depth || has_depth);
        }
    };

    struct FrameBufferColorView {
        void *native_handle = nullptr;

        [[nodiscard]] bool IsValid() const {
            return native_handle != nullptr;
        }
    };

    struct FrameBufferDepthView {
        void *native_handle = nullptr;

        [[nodiscard]] bool IsValid() const {
            return native_handle != nullptr;
        }
    };
} // namespace CoreEngine
