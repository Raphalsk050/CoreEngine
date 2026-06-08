#pragma once

#include "core/render/render_pass.h"
#include "core/render/render_system.h"

namespace CoreEngine {
    /**
     * @brief Publishes PBR image-based lighting resources for the current frame.
     *
     * Responsibility: isolate environment/probe resource publication from scene
     * geometry rendering and from backend-specific texture handles.
     */
    class PbrIblPass final : public IRenderPass {
    public:
        explicit PbrIblPass(RenderSystem &owner) : owner_(owner) {}

        [[nodiscard]] RenderPassDesc Describe() const override {
            return {.name = "PbrIbl", .stage = RenderPassStage::FrameSetup, .order = 10};
        }

        void Execute(RenderPassContext &context) override;

    private:
        RenderSystem &owner_;
    };
} // namespace CoreEngine
