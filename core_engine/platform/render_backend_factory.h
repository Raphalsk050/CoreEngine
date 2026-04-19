#pragma once

#include <memory>

#include "core/render/i_render_backend.h"
#include "core/render/render_backend_type.h"

namespace CoreEngine {
    [[nodiscard]] std::unique_ptr<IRenderBackend> CreateRenderBackend(RenderBackendType backend_type);
} // namespace CoreEngine
