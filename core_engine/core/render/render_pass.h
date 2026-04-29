#pragma once

#include <cstdint>

#include "core/render/frame_buffer.h"
#include "core/render/i_render_backend.h"
#include "core/render/render_batch.h"
#include "core/render/render_clear_color.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    class FrameClock;
    class World;

    enum class RenderPassStage {
        BeforeMainScene,
        MainScene,
        AfterMainScene,
        BeforeImGui,
    };

    struct RenderPassDesc {
        const char *name = "RenderPass";
        RenderPassStage stage = RenderPassStage::BeforeMainScene;
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
                          RenderFrameTiming timing)
            : backend_(backend), world_(world), frame_clock_(frame_clock), timing_(timing) {
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
    };

    class IRenderPass {
    public:
        virtual ~IRenderPass() = default;

        [[nodiscard]] virtual RenderPassDesc Describe() const = 0;

        virtual void Execute(RenderPassContext &context) = 0;
    };
} // namespace CoreEngine
