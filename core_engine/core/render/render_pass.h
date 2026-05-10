#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "render_frame_resources.h"
#include "core/render/frame_buffer.h"
#include "core/render/i_render_backend.h"
#include "core/render/render_batch.h"
#include "core/render/render_clear_color.h"
#include "core/render/render_handle.h"
#include "debug/depth_visualization.h"

namespace CoreEngine {
    class FrameClock;
    class World;

    //clang-format off
    enum class RenderPassStage {
        FrameSetup,                 // Per-frame setup before any scene rendering.
        Shadow,                     // Renders shadows maps and other light-space depth resources
        DepthPrePass,               // Fills scene depth before color rendering
        GBuffer,                    // Writes deferred rendering geometry buffers
        Lighting,                   // Computes lighting from scene/material buffers
        ForwardOpaque,              // Renders opaque forward geometry
        ForwardTransparent,         // Renders transparent forward geometry after opaque
        PostProcess,                // Applies fullscreen effects after scene rendering
        Debug,                      // Produces debug overlays or debug textures
        UI,                         // renders the ui, generally imgui
        Present                     // the final rendering
    };
    //clang-format on

    struct RenderPassDesc {
        const char *name = "RenderPass";
        RenderPassStage stage = RenderPassStage::FrameSetup;
        int order = 0;
    };

    struct RenderFrameTiming {
        float delta_seconds = 0.0f;
        double total_seconds = 0.0;
        std::uint64_t frame_index = 0;
    };

    class RenderPassContext final {
    public:
        RenderPassContext(IRenderBackend &backend,
                          World &world,
                          const FrameClock &frame_clock,
                          RenderFrameTiming timing,
                          RenderFrameResources &frame_resources,
                          int surface_width,
                          int surface_height)
            : backend_(backend), world_(world), frame_clock_(frame_clock), timing_(timing),
              surface_width_(surface_width), surface_height_(surface_height),
              frame_resources_(frame_resources) {
        }

        [[nodiscard]] World &GetWorld() const { return world_; }

        [[nodiscard]] const FrameClock &GetFrameClock() const { return frame_clock_; }

        [[nodiscard]] RenderFrameTiming GetFrameTiming() const { return timing_; }

        [[nodiscard]] float DeltaSeconds() const { return timing_.delta_seconds; }

        [[nodiscard]] double TotalSeconds() const { return timing_.total_seconds; }

        [[nodiscard]] std::uint64_t FrameIndex() const { return timing_.frame_index; }

        [[nodiscard]] FrameBufferHandle CreateFrameBuffer(const FrameBufferDesc &desc) const {
            return backend_.CreateFrameBuffer(desc);
        }

        void DestroyFrameBuffer(FrameBufferHandle handle) const {
            backend_.DestroyFrameBuffer(handle);
        }

        void SetFrameBuffer(FrameBufferHandle handle) const {
            backend_.SetFrameBuffer(handle);
        }

        void SetSwapChainFrameBuffer() const {
            backend_.SetSwapChainFrameBuffer();
        }

        [[nodiscard]] ShaderProgramHandle CreateShaderProgram(const ShaderProgramDesc &desc) const {
            return backend_.CreateShaderProgram(desc);
        }

        void DestroyShaderProgram(ShaderProgramHandle handle) const {
            backend_.DestroyShaderProgram(handle);
        }

        void UseShaderProgram(ShaderProgramHandle handle) const {
            backend_.UseShaderProgram(handle);
        }

        void BindTexture(std::string_view name, TextureHandle handle) const {
            backend_.BindShaderTexture(name, handle);
        }

        void BindTexture(std::string_view name, FrameBufferColorView view) const {
            backend_.BindShaderTexture(name, view);
        }

        void BindTexture(std::string_view name, FrameBufferDepthView view) const {
            backend_.BindShaderTexture(name, view);
        }

        void BindUniform(std::string_view name, std::span<const std::uint8_t> data) const {
            backend_.BindShaderUniform(name, data);
        }

        template<typename T>
        void BindUniform(std::string_view name, const T &data) const {
            BindUniform(name,
                        std::span<const std::uint8_t>(
                            reinterpret_cast<const std::uint8_t *>(&data),
                            sizeof(T)));
        }

        void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1) const {
            backend_.Draw(vertex_count, instance_count);
        }

        void DrawFullscreenTriangle() const {
            Draw(3u, 1u);
        }

        void SetGlobalColorTexture(GlobalTextureSlot slot, FrameBufferColorView view) {
            frame_resources_.SetColorTexture(slot, view);
        }

        void SetGlobalDepthTexture(GlobalTextureSlot slot, FrameBufferDepthView view) {
            frame_resources_.SetDepthTexture(slot, view);
        }

        [[nodiscard]] FrameBufferColorView GetGlobalColorTexture(GlobalTextureSlot slot) const {
            return frame_resources_.GetColorTexture(slot);
        }

        [[nodiscard]] FrameBufferDepthView GetGlobalDepthTexture(GlobalTextureSlot slot) const {
            return frame_resources_.GetDepthTexture(slot);
        }

        void RenderDepthToColor(FrameBufferDepthView source, FrameBufferHandle destination,
                                const DepthVisualizationDesc &desc = {}) const {
            backend_.RenderDepthToColor(source, destination, desc);
        }

        [[nodiscard]] int SurfaceWidth() const {
            return surface_width_;
        }

        [[nodiscard]] int SurfaceHeight() const {
            return surface_height_;
        }

        void Clear(const RenderClearColor &clear_color) const {
            backend_.Clear(clear_color);
        }

        [[nodiscard]] FrameBufferColorView GetFrameBufferColorView(FrameBufferHandle handle) const {
            return backend_.GetFrameBufferColorView(handle);
        }

        [[nodiscard]] FrameBufferDepthView GetFrameBufferDepthView(FrameBufferHandle handle) const {
            return backend_.GetFrameBufferDepthView(handle);
        }

        void SetPerFrameProps(PerFrameProps props) const {
            backend_.SetPerFrameProps(props);
        }

        void SubmitBatch(const RenderBatch &batch) const {
            backend_.SubmitBatch(batch);
        }

    private:
        IRenderBackend &backend_;
        World &world_;
        const FrameClock &frame_clock_;
        RenderFrameTiming timing_;
        RenderFrameResources &frame_resources_;
        int surface_width_ = 1;
        int surface_height_ = 1;
    };

    class IRenderPass {
    public:
        virtual ~IRenderPass() = default;

        virtual void ReleaseResources(IRenderBackend &backend) {
            (void) backend;
        }

        [[nodiscard]] virtual RenderPassDesc Describe() const = 0;

        virtual void Execute(RenderPassContext &context) = 0;
    };
} // namespace CoreEngine
