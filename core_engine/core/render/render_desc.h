#pragma once

#include <cstdint>

#include "core/render/frame_buffer.h"
#include "core/render/render_backend_type.h"
#include "core/render/render_clear_color.h"

namespace CoreEngine {
    enum class ToneMappingOperator : std::uint32_t {
        None = 0,
        Reinhard = 1,
        AcesFilmic = 2,
    };

    /**
     * @brief Controls screen-space mapping from scene-linear HDR color to swapchain output.
     */
    struct PostProcessDesc {
        float exposure = 1.0f;
        ToneMappingOperator tone_mapping = ToneMappingOperator::AcesFilmic;
    };

    struct RenderDesc {
        RenderBackendType backend = RenderBackendType::None;
        RenderClearColor clear_color{};
        bool vsync = true;
        bool enable_imgui = true;
        FrameBufferFormat scene_color_format = FrameBufferFormat::RGBA16Float;
        PostProcessDesc post_process{};
        // Explicit swapchain dimensions.
        // Must match the window size at initialization time.
        // Passing 0 lets the backend infer from the HWND (may fail on some drivers).
        int width = 0;
        int height = 0;
    };
} // namespace CoreEngine
