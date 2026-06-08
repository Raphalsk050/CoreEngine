#include "core/render/render_pass/pbr_ibl_pass.h"

namespace CoreEngine {
    void PbrIblPass::Execute(RenderPassContext &context) { owner_.ExecutePbrIblPass(context); }
} // namespace CoreEngine
