#include "core/render/render_pass/pbr_debug_pass.h"

namespace CoreEngine {
    void PbrDebugPass::Execute(RenderPassContext &context) { owner_.ExecutePbrDebugPass(context); }
} // namespace CoreEngine
