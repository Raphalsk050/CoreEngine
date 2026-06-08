#include "core/render/render_pass/pbr_shadow_pass.h"

namespace CoreEngine {
    void PbrShadowPass::Execute(RenderPassContext &context) { owner_.ExecutePbrShadowPass(context); }
} // namespace CoreEngine
