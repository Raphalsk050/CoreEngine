#include "core/render/render_pass/default_scene_render_pass.h"

namespace CoreEngine {
    void DefaultSceneRenderPass::Execute(RenderPassContext &context) {
        owner_.ExecuteDefaultScenePass(context);
    }
} // CoreEngine
