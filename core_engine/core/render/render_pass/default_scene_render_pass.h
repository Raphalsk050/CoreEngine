#pragma once
#include "core/render/render_pass.h"
#include "core/render/render_system.h"

namespace CoreEngine {
    class DefaultSceneRenderPass final : public IRenderPass {
    public:
        explicit DefaultSceneRenderPass(RenderSystem &owner) : owner_(owner) {}

        [[nodiscard]] RenderPassDesc Describe() const override {
            return {.name = "Default", .stage = RenderPassStage::ForwardOpaque, .order = 0};
        }

        void Execute(RenderPassContext &context) override;

    private:
        RenderSystem &owner_;
    };
} // namespace CoreEngine
