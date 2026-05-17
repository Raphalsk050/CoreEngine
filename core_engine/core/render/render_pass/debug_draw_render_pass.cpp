#include "core/render/render_pass/debug_draw_render_pass.h"

namespace CoreEngine {
    void DebugDrawRenderPass::Execute(RenderPassContext &context) {
        owner_.ExecuteDebugDrawPass(context);
    }
} // namespace CoreEngine
