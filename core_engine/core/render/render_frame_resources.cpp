#include "core/render/render_frame_resources.h"

#include <cstdio>

namespace CoreEngine {
    namespace {
        constexpr std::size_t ToIndex(GlobalTextureSlot slot) {
            return static_cast<std::size_t>(slot);
        }

        constexpr bool IsValidSlot(GlobalTextureSlot slot) {
            return ToIndex(slot) < static_cast<std::size_t>(GlobalTextureSlot::Count);
        }
    }

    void RenderFrameResources::SetColorTexture(GlobalTextureSlot slot, FrameBufferColorView view) {
        if (!IsValidSlot(slot)) {
            return;
        }

        color_textures_[ToIndex(slot)] = view;
    }

    void RenderFrameResources::SetDepthTexture(GlobalTextureSlot slot, FrameBufferDepthView view) {
        if (!IsValidSlot(slot)) {
            return;
        }
        depth_textures_[ToIndex(slot)] = view;
    }

    FrameBufferColorView RenderFrameResources::GetColorTexture(GlobalTextureSlot slot) const {
        if (!IsValidSlot(slot)) {
            return {};
        }
        return color_textures_[ToIndex(slot)];
    }

    FrameBufferDepthView RenderFrameResources::GetDepthTexture(GlobalTextureSlot slot) const {
        if (!IsValidSlot(slot)) {
            return {};
        }
        return depth_textures_[ToIndex(slot)];
    }

    void RenderFrameResources::Clear() {
        color_textures_.fill({});
        depth_textures_.fill({});
    }
} // CoreEngine
