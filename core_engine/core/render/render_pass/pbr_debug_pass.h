#pragma once

#include "core/render/render_pass.h"
#include "core/render/render_system.h"

namespace CoreEngine {
    /**
     * @brief Displays a selected visual debug view without coupling features together.
     *
     * Responsibility: consume entries registered by render passes and present the
     * selected view through the existing scene framebuffer.
     */
    class PbrDebugPass final : public IRenderPass {
    public:
        explicit PbrDebugPass(RenderSystem &owner) : owner_(owner) {}

        [[nodiscard]] RenderPassDesc Describe() const override {
            return {.name = "PbrDebug", .stage = RenderPassStage::Debug, .order = 100};
        }

        void Execute(RenderPassContext &context) override;

    private:
        RenderSystem &owner_;
    };
} // namespace CoreEngine
