#include "platform/render_backend_factory.h"

#include "core/render/null_render_backend.h"

#ifndef CORE_ENGINE_ENABLE_DILIGENT
#define CORE_ENGINE_ENABLE_DILIGENT 0
#endif

#if CORE_ENGINE_ENABLE_DILIGENT
#include "platform/diligent/diligent_render_backend.h"
#endif

#ifndef PLATFORM_WIN32
#define PLATFORM_WIN32 0
#endif

#ifndef PLATFORM_MACOS
#define PLATFORM_MACOS 0
#endif

#ifndef D3D11_SUPPORTED
#define D3D11_SUPPORTED 0
#endif

#ifndef D3D12_SUPPORTED
#define D3D12_SUPPORTED 0
#endif

#ifndef VULKAN_SUPPORTED
#define VULKAN_SUPPORTED 0
#endif

#ifndef CORE_ENGINE_HAS_DILIGENT_D3D11
#define CORE_ENGINE_HAS_DILIGENT_D3D11 0
#endif

#ifndef CORE_ENGINE_HAS_DILIGENT_D3D12
#define CORE_ENGINE_HAS_DILIGENT_D3D12 0
#endif

#ifndef CORE_ENGINE_HAS_DILIGENT_VULKAN
#define CORE_ENGINE_HAS_DILIGENT_VULKAN 0
#endif

namespace CoreEngine {
    bool IsRenderBackendAvailable(RenderBackendType backend_type) {
        switch (backend_type) {
            case RenderBackendType::None: return true;

            case RenderBackendType::DiligentD3D11:
                return CORE_ENGINE_ENABLE_DILIGENT && PLATFORM_WIN32 && CORE_ENGINE_HAS_DILIGENT_D3D11;

            case RenderBackendType::DiligentD3D12:
                return CORE_ENGINE_ENABLE_DILIGENT && PLATFORM_WIN32 && CORE_ENGINE_HAS_DILIGENT_D3D12;

            case RenderBackendType::DiligentVulkan:
                return CORE_ENGINE_ENABLE_DILIGENT && CORE_ENGINE_HAS_DILIGENT_VULKAN &&
                       (PLATFORM_WIN32 || PLATFORM_MACOS);
        }

        return false;
    }

    RenderBackendType SelectAvailableRenderBackend(RenderBackendType preferred_backend) {
        if (IsRenderBackendAvailable(preferred_backend)) {
            return preferred_backend;
        }

        return RenderBackendType::None;
    }

    std::unique_ptr<IRenderBackend> CreateRenderBackend(RenderBackendType backend_type) {
        switch (backend_type) {
            case RenderBackendType::None: return std::make_unique<NullRenderBackend>();

            case RenderBackendType::DiligentD3D11:
#if CORE_ENGINE_ENABLE_DILIGENT && PLATFORM_WIN32 && CORE_ENGINE_HAS_DILIGENT_D3D11
                return std::make_unique<DiligentRenderBackend>(DiligentRenderBackendApi::D3D11);
#else
                return nullptr;
#endif

            case RenderBackendType::DiligentD3D12:
#if CORE_ENGINE_ENABLE_DILIGENT && PLATFORM_WIN32 && CORE_ENGINE_HAS_DILIGENT_D3D12
                return std::make_unique<DiligentRenderBackend>(DiligentRenderBackendApi::D3D12);
#else
                return nullptr;
#endif

            case RenderBackendType::DiligentVulkan:
#if CORE_ENGINE_ENABLE_DILIGENT && CORE_ENGINE_HAS_DILIGENT_VULKAN && (PLATFORM_WIN32 || PLATFORM_MACOS)
                return std::make_unique<DiligentRenderBackend>(DiligentRenderBackendApi::Vulkan);
#else
                return nullptr;
#endif
        }

        return nullptr;
    }
} // namespace CoreEngine
