#pragma once
#include <array>

#include "core/render/frame_buffer.h"

// Stores all the resources that were published in the current frame
namespace CoreEngine {
    enum class GlobalTextureSlot {
        SceneColor,
        SceneDepth,
        SceneDepthDebugColor,
        Count,
    };


    class RenderFrameResources {
    public:
        void SetColorTexture(GlobalTextureSlot slot, FrameBufferColorView view);

        void SetDepthTexture(GlobalTextureSlot slot, FrameBufferDepthView view);

        [[nodiscard]] FrameBufferColorView GetColorTexture(GlobalTextureSlot slot) const;

        [[nodiscard]] FrameBufferDepthView GetDepthTexture(GlobalTextureSlot slot) const;

        void Clear();

    private:
        static constexpr std::size_t kTextureSlotCount = static_cast<std::size_t>(GlobalTextureSlot::Count);
        std::array<FrameBufferColorView, kTextureSlotCount> color_textures_{};
        std::array<FrameBufferDepthView, kTextureSlotCount> depth_textures_{};
    };
} // namespace CoreEngine
