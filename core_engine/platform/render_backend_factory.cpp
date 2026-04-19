#include "platform/render_backend_factory.h"

#include "core/render/null_render_backend.h"

#ifndef CORE_ENGINE_ENABLE_DILIGENT
#define CORE_ENGINE_ENABLE_DILIGENT 0
#endif

#if CORE_ENGINE_ENABLE_DILIGENT
#include "platform/diligent/diligent_render_backend.h"
#endif

namespace CoreEngine {
    std::unique_ptr<IRenderBackend> CreateRenderBackend(RenderBackendType backend_type) {
        switch (backend_type) {
            case RenderBackendType::None:
                return std::make_unique<NullRenderBackend>();

            case RenderBackendType::DiligentD3D11:
#if CORE_ENGINE_ENABLE_DILIGENT
                return std::make_unique<DiligentRenderBackend>(DiligentRenderBackendApi::D3D11);
#else
                return nullptr;
#endif

            case RenderBackendType::DiligentD3D12:
#if CORE_ENGINE_ENABLE_DILIGENT
                return std::make_unique<DiligentRenderBackend>(DiligentRenderBackendApi::D3D12);
#else
                return nullptr;
#endif
        }

        return nullptr;
    }
} // namespace CoreEngine
