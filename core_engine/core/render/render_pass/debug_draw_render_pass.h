#pragma once

#include "core/render/render_pass.h"
#include "core/render/render_system.h"

namespace CoreEngine {
    class DebugDrawRenderPass final : public IRenderPass {
    public:
        explicit DebugDrawRenderPass(RenderSystem &owner) : owner_(owner) {
        }

        [[nodiscard]] RenderPassDesc Describe() const override {
            return {
                .name = "DebugDraw",
                .stage = RenderPassStage::Debug,
                .order = 0,
            };
        }

        void Execute(RenderPassContext &context) override;

    private:
        RenderSystem &owner_;
    };
} // namespace CoreEngine
