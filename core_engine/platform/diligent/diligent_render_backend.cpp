#include "platform/diligent/diligent_render_backend.h"

#include <array>
#include <string>

#include "EngineFactoryD3D11.h"
#include "EngineFactoryD3D12.h"
#include "DiligentCore/Common/interface/RefCntAutoPtr.hpp"

namespace CoreEngine {
    struct DiligentRenderBackend::Impl {
        explicit Impl(DiligentRenderBackendApi api_type)
            : api(api_type) {
        }

        DiligentRenderBackendApi api = DiligentRenderBackendApi::D3D11;
        RenderDesc desc{};
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> immediate_context;
        Diligent::RefCntAutoPtr<Diligent::ISwapChain> swap_chain;
        std::string last_error;
    };

    namespace {
        /**
         * Creates a SwapChainDesc with safe defaults.
         * IMPORTANT: DXGI requires BufferCount >= 2 when using the default
         * SWAP_EFFECT_DISCARD mode. Passing 1 causes E_INVALIDARG (the exact
         * error seen: "O aplicativo fez uma chamada inválida").
         * Width/Height are supplied explicitly so Diligent does not attempt to
         * query them from the HWND before the window is fully presented.
         */
        [[nodiscard]] Diligent::SwapChainDesc CreateSwapChainDesc(const RenderDesc &render_desc) {
            Diligent::SwapChainDesc desc;
            // Minimum 2 back-buffers required by DXGI for SWAP_EFFECT_DISCARD.
            desc.BufferCount  = 2;
            desc.Width        = static_cast<Diligent::Uint32>(render_desc.width);
            desc.Height       = static_cast<Diligent::Uint32>(render_desc.height);
            return desc;
        }

        [[nodiscard]] bool IsSupportedWindow(NativeWindowHandle native_window) {
            return native_window.platform == NativeWindowPlatform::Win32 && native_window.IsValid();
        }
    } // namespace

    DiligentRenderBackend::DiligentRenderBackend(DiligentRenderBackendApi api)
        : impl_(std::make_unique<Impl>(api)) {
    }

    DiligentRenderBackend::~DiligentRenderBackend() {
        Shutdown();
    }

    bool DiligentRenderBackend::Initialize(const RenderDesc &desc, NativeWindowHandle native_window) {
        impl_->desc = desc;

        if (!IsSupportedWindow(native_window)) {
            impl_->last_error = "Diligent backend requires a valid Win32 native window handle";
            return false;
        }

        const Diligent::SwapChainDesc swap_chain_desc = CreateSwapChainDesc(impl_->desc);
        const Diligent::FullScreenModeDesc fullscreen_desc{};
        const Diligent::Win32NativeWindow window{native_window.window};

        switch (impl_->api) {
            case DiligentRenderBackendApi::D3D11: {
                Diligent::IEngineFactoryD3D11 *factory = Diligent::LoadAndGetEngineFactoryD3D11();
                if (factory == nullptr) {
                    impl_->last_error = "Failed to load Diligent D3D11 engine factory";
                    return false;
                }

                Diligent::EngineD3D11CreateInfo engine_create_info;
                factory->CreateDeviceAndContextsD3D11(engine_create_info, &impl_->device, &impl_->immediate_context);
                if (!impl_->device || !impl_->immediate_context) {
                    impl_->last_error = "Failed to create Diligent D3D11 device/context";
                    return false;
                }

                factory->CreateSwapChainD3D11(
                    impl_->device,
                    impl_->immediate_context,
                    swap_chain_desc,
                    fullscreen_desc,
                    window,
                    &impl_->swap_chain);
                break;
            }

            case DiligentRenderBackendApi::D3D12: {
                Diligent::IEngineFactoryD3D12 *factory = Diligent::LoadAndGetEngineFactoryD3D12();
                if (factory == nullptr) {
                    impl_->last_error = "Failed to load Diligent D3D12 engine factory";
                    return false;
                }

                Diligent::EngineD3D12CreateInfo engine_create_info;
                factory->CreateDeviceAndContextsD3D12(engine_create_info, &impl_->device, &impl_->immediate_context);
                if (!impl_->device || !impl_->immediate_context) {
                    impl_->last_error = "Failed to create Diligent D3D12 device/context";
                    return false;
                }

                factory->CreateSwapChainD3D12(
                    impl_->device,
                    impl_->immediate_context,
                    swap_chain_desc,
                    fullscreen_desc,
                    window,
                    &impl_->swap_chain);
                break;
            }
        }

        if (!impl_->swap_chain) {
            impl_->last_error = "Failed to create Diligent swap chain";
            return false;
        }

        return true;
    }

    void DiligentRenderBackend::BeginFrame() {
    }

    void DiligentRenderBackend::Clear(const RenderClearColor &clear_color) {
        if (!impl_->swap_chain || !impl_->immediate_context) {
            return;
        }

        Diligent::ITextureView *render_target = impl_->swap_chain->GetCurrentBackBufferRTV();
        if (render_target == nullptr) {
            return;
        }

        Diligent::ITextureView *render_targets[] = {render_target};
        impl_->immediate_context->SetRenderTargets(
            1,
            render_targets,
            nullptr,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const std::array<float, 4> rgba{
            clear_color.r,
            clear_color.g,
            clear_color.b,
            clear_color.a
        };

        impl_->immediate_context->ClearRenderTarget(
            render_target,
            rgba.data(),
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    void DiligentRenderBackend::EndFrame() {
        if (!impl_->swap_chain) {
            return;
        }

        impl_->swap_chain->Present(impl_->desc.vsync ? 1u : 0u);
    }

    void DiligentRenderBackend::Resize(int width, int height) {
        if (!impl_->swap_chain || width <= 0 || height <= 0) {
            return;
        }

        impl_->swap_chain->Resize(static_cast<Diligent::Uint32>(width), static_cast<Diligent::Uint32>(height));
    }

    void DiligentRenderBackend::Shutdown() {
        if (impl_ == nullptr) {
            return;
        }

        impl_->swap_chain.Release();
        impl_->immediate_context.Release();
        impl_->device.Release();
    }

    std::string_view DiligentRenderBackend::LastError() const {
        return impl_ != nullptr ? impl_->last_error : "Diligent backend is not available";
    }
} // namespace CoreEngine
