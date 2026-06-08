#pragma once

#include "core/render/render_pass.h"
#include "core/render/render_system.h"

namespace CoreEngine {
    /**
     * @brief Renders PBR shadow depth resources before scene color rendering.
     *
     * Responsibility: keep shadow map production as a modular render-graph pass
     * while RenderSystem owns shared scene/resource state.
     */
    class PbrShadowPass final : public IRenderPass {
    public:
        explicit PbrShadowPass(RenderSystem &owner) : owner_(owner) {}

        [[nodiscard]] RenderPassDesc Describe() const override {
            return {.name = "PbrShadow", .stage = RenderPassStage::Shadow, .order = 0};
        }

        void Execute(RenderPassContext &context) override;

    private:
        RenderSystem &owner_;
    };
} // namespace CoreEngine
