#include "platform/diligent/diligent_render_backend.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EngineFactoryVk.h"

#if PLATFORM_WIN32
#include "EngineFactoryD3D11.h"
#include "EngineFactoryD3D12.h"
#endif
#include "DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "core/render/render_batch.h"
#include "core/render/vertex.h"

#include "core/math/math.h"

namespace CoreEngine {
    namespace {
        struct PerFrameCB {
            Math::Mat4 view_proj;
        };

        struct PerObjectCB {
            Math::Mat4 model;
        };

        struct DiligentMeshData {
            Diligent::RefCntAutoPtr<Diligent::IBuffer> vertex_buffer;
            Diligent::RefCntAutoPtr<Diligent::IBuffer> index_buffer;
            uint32_t index_count = 0;
            uint32_t generation = 0;
            IndexFormat index_format = IndexFormat::UInt32;
        };

        struct DiligentMaterialData {
            Diligent::RefCntAutoPtr<Diligent::IPipelineState> pso;
            Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
            Diligent::RefCntAutoPtr<Diligent::IBuffer> material_cbuffer;
            std::vector<uint8_t> properties_data;
        };
    }

    struct DiligentRenderBackend::Impl {
        explicit Impl(DiligentRenderBackendApi api_type) : api(api_type) {
        }

        DiligentRenderBackendApi api;
        RenderDesc desc{};
        std::string last_error;

        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> immediate_context;
        Diligent::RefCntAutoPtr<Diligent::ISwapChain> swap_chain;

        Diligent::RefCntAutoPtr<Diligent::IBuffer> per_frame_cb;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> per_object_cb;

        std::unordered_map<uint32_t, DiligentMeshData> mesh_registry;
        std::unordered_map<uint32_t, DiligentMaterialData> material_registry;
        std::unordered_map<uint64_t, MaterialHandle> material_hash_cache;

        uint32_t next_mesh_id = 1;
        uint32_t next_material_id = 1;
        uint32_t mesh_generation = 1;
    };

    namespace {
        bool IsSupportedWindow(NativeWindowHandle h, DiligentRenderBackendApi api) {
            if (!h.IsValid()) {
                return false;
            }

            switch (api) {
                case DiligentRenderBackendApi::D3D11:
                case DiligentRenderBackendApi::D3D12:
#if PLATFORM_WIN32
                    return h.platform == NativeWindowPlatform::Win32;
#else
                    return false;
#endif

                case DiligentRenderBackendApi::Vulkan:
#if PLATFORM_WIN32
                    return h.platform == NativeWindowPlatform::Win32;
#elif PLATFORM_MACOS
                    return h.platform == NativeWindowPlatform::MacOS;
#else
                    return false;
#endif

                case DiligentRenderBackendApi::Metal:
#if PLATFORM_MACOS
                    return h.platform == NativeWindowPlatform::MacOS;
#else
                    return false;
#endif
            }

            return false;
        }

        const char *UnsupportedWindowMessage(DiligentRenderBackendApi api) {
            switch (api) {
                case DiligentRenderBackendApi::D3D11:
                case DiligentRenderBackendApi::D3D12:
                    return "Diligent D3D backend requires a valid Win32 HWND";
                case DiligentRenderBackendApi::Vulkan:
#if PLATFORM_MACOS
                    return "Diligent Vulkan backend requires a valid macOS NSView";
#else
                    return "Diligent Vulkan backend requires a valid native window";
#endif
                case DiligentRenderBackendApi::Metal:
                    return "Diligent Metal backend is not implemented";
            }

            return "Unsupported render backend window";
        }

        Diligent::NativeWindow MakeNativeWindow(NativeWindowHandle h) {
#if PLATFORM_WIN32 || PLATFORM_MACOS
            return Diligent::NativeWindow{h.window};
#else
            (void) h;
            return Diligent::NativeWindow{};
#endif
        }

        Diligent::SwapChainDesc MakeSwapChainDesc(const RenderDesc &d) {
            Diligent::SwapChainDesc desc;
            desc.BufferCount = 2;
            desc.Width = static_cast<Diligent::Uint32>(d.width);
            desc.Height = static_cast<Diligent::Uint32>(d.height);
            return desc;
        }

        Diligent::RefCntAutoPtr<Diligent::IBuffer> CreateConstantBuffer(
            Diligent::IRenderDevice *device, uint32_t byte_size, const char *name) {
            Diligent::BufferDesc desc;
            desc.Name = name;
            desc.Size = byte_size;
            desc.Usage = Diligent::USAGE_DYNAMIC;
            desc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
            desc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;

            Diligent::RefCntAutoPtr<Diligent::IBuffer> buf;
            device->CreateBuffer(desc, nullptr, &buf);
            return buf;
        }

        bool CreateSharedConstantBuffers(DiligentRenderBackend::Impl &impl) {
            impl.per_frame_cb = CreateConstantBuffer(impl.device, sizeof(PerFrameCB), "PerFrame");
            impl.per_object_cb = CreateConstantBuffer(impl.device, sizeof(PerObjectCB), "PerObject");
            return impl.per_frame_cb && impl.per_object_cb;
        }

        Diligent::RefCntAutoPtr<Diligent::IBuffer> CreateGpuBuffer(
            Diligent::IRenderDevice *device,
            const void *data,
            uint32_t byte_size,
            Diligent::BIND_FLAGS flags,
            const char *name) {
            Diligent::BufferDesc desc;
            desc.Name = name;
            desc.Size = byte_size;
            desc.Usage = Diligent::USAGE_IMMUTABLE;
            desc.BindFlags = flags;

            Diligent::BufferData buf_data;
            buf_data.pData = data;
            buf_data.DataSize = byte_size;

            Diligent::RefCntAutoPtr<Diligent::IBuffer> buf;
            device->CreateBuffer(desc, &buf_data, &buf);
            return buf;
        }

        uint32_t AlignConstantBufferSize(uint32_t byte_size) {
            constexpr uint32_t kAlignment = 256;
            return (byte_size + kAlignment - 1u) & ~(kAlignment - 1u);
        }

        Diligent::RefCntAutoPtr<Diligent::IBuffer> CreateImmutableConstantBuffer(
            Diligent::IRenderDevice *device,
            std::span<const uint8_t> data,
            const char *name) {
            if (data.empty()) {
                return {};
            }

            std::vector<uint8_t> padded_data(data.begin(), data.end());
            padded_data.resize(AlignConstantBufferSize(static_cast<uint32_t>(padded_data.size())), 0u);

            Diligent::BufferDesc desc;
            desc.Name = name;
            desc.Size = static_cast<Diligent::Uint64>(padded_data.size());
            desc.Usage = Diligent::USAGE_IMMUTABLE;
            desc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;

            Diligent::BufferData buffer_data;
            buffer_data.pData = padded_data.data();
            buffer_data.DataSize = static_cast<Diligent::Uint64>(padded_data.size());

            Diligent::RefCntAutoPtr<Diligent::IBuffer> buffer;
            device->CreateBuffer(desc, &buffer_data, &buffer);
            return buffer;
        }

        DiligentMeshData UploadMeshData(Diligent::IRenderDevice *device, const MeshDesc &desc) {
            DiligentMeshData mesh;
            mesh.index_count = static_cast<uint32_t>(desc.indices.size());
            mesh.index_format = desc.index_format;

            mesh.vertex_buffer = CreateGpuBuffer(
                device,
                desc.vertices.data(),
                static_cast<uint32_t>(desc.vertices.size_bytes()),
                Diligent::BIND_VERTEX_BUFFER,
                "StaticMeshVB");

            mesh.index_buffer = CreateGpuBuffer(
                device,
                desc.indices.data(),
                static_cast<uint32_t>(desc.indices.size_bytes()),
                Diligent::BIND_INDEX_BUFFER,
                "StaticMeshIB");

            return mesh;
        }

        Diligent::RefCntAutoPtr<Diligent::IShader> CompileShader(
            Diligent::IRenderDevice *device,
            Diligent::SHADER_TYPE type,
            const char *source,
            const char *name) {
            Diligent::ShaderCreateInfo sci;
            sci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            sci.Desc.ShaderType = type;
            sci.Desc.Name = name;
            sci.Source = source;
            sci.EntryPoint = "main";

            Diligent::RefCntAutoPtr<Diligent::IShader> shader;
            device->CreateShader(sci, &shader);
            return shader;
        }

        DiligentMaterialData CreateMaterial(
            DiligentRenderBackend::Impl &impl,
            const MaterialDesc &desc) {
            auto vs = CompileShader(impl.device, Diligent::SHADER_TYPE_VERTEX,
                                    desc.vertex_shader_source.c_str(), "VS");
            auto ps = CompileShader(impl.device, Diligent::SHADER_TYPE_PIXEL,
                                    desc.pixel_shader_source.c_str(), "PS");

            // NOTE: Explicit offsets required — GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
            // changes sizeof(glm::vec3) from 12 to 16, adding padding between fields.
            constexpr Diligent::Uint32 kStride = sizeof(StaticMeshVertex);

            Diligent::LayoutElement layout[] = {
                // InputIndex, BufferSlot, NumComponents, ValueType, IsNormalized,
                //   RelativeOffset, Stride
                {0, 0, 3, Diligent::VT_FLOAT32, false,
                    static_cast<Diligent::Uint32>(offsetof(StaticMeshVertex, position)), kStride},
                {1, 0, 3, Diligent::VT_FLOAT32, false,
                    static_cast<Diligent::Uint32>(offsetof(StaticMeshVertex, normal)),   kStride},
                {2, 0, 3, Diligent::VT_FLOAT32, false,
                    static_cast<Diligent::Uint32>(offsetof(StaticMeshVertex, color)),    kStride},
                {3, 0, 2, Diligent::VT_FLOAT32, false,
                    static_cast<Diligent::Uint32>(offsetof(StaticMeshVertex, uv)),       kStride},
            };

            Diligent::GraphicsPipelineStateCreateInfo pci;
            pci.PSODesc.Name = "Material";
            pci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
            pci.GraphicsPipeline.NumRenderTargets = 1;
            pci.GraphicsPipeline.RTVFormats[0] = impl.swap_chain->GetDesc().ColorBufferFormat;
            pci.GraphicsPipeline.DSVFormat = impl.swap_chain->GetDesc().DepthBufferFormat;
            pci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            pci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_BACK;
            pci.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
            pci.GraphicsPipeline.InputLayout.LayoutElements = layout;
            pci.GraphicsPipeline.InputLayout.NumElements = 4;
            pci.pVS = vs;
            pci.pPS = ps;

            Diligent::ShaderResourceVariableDesc vars[] = {
                {Diligent::SHADER_TYPE_VERTEX, "PerFrame", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                {Diligent::SHADER_TYPE_VERTEX, "PerObject", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                {Diligent::SHADER_TYPE_PIXEL, "PerMaterial", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
            };
            pci.PSODesc.ResourceLayout.Variables = vars;
            pci.PSODesc.ResourceLayout.NumVariables = 3;

            DiligentMaterialData mat;
            impl.device->CreateGraphicsPipelineState(pci, &mat.pso);

            if (!mat.pso) {
                return mat;
            }

            mat.pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "PerFrame")
                    ->Set(impl.per_frame_cb);
            mat.pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "PerObject")
                    ->Set(impl.per_object_cb);

            if (!desc.properties_data.empty()) {
                mat.material_cbuffer = CreateImmutableConstantBuffer(
                    impl.device,
                    desc.properties_data,
                    "PerMaterial");

                mat.pso->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "PerMaterial")
                        ->Set(mat.material_cbuffer);
            }

            mat.properties_data = desc.properties_data;
            mat.pso->CreateShaderResourceBinding(&mat.srb, true);

            return mat;
        }

        void UpdateBuffer(Diligent::IDeviceContext *ctx,
                          Diligent::IBuffer *buf,
                          const void *data,
                          uint32_t byte_size) {
            void *mapped = nullptr;
            ctx->MapBuffer(buf, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
            if (mapped) {
                std::memcpy(mapped, data, byte_size);
                ctx->UnmapBuffer(buf, Diligent::MAP_WRITE);
            }
        }
    }

    DiligentRenderBackend::DiligentRenderBackend(DiligentRenderBackendApi api)
        : impl_(std::make_unique<Impl>(api)) {
    }

    DiligentRenderBackend::~DiligentRenderBackend() {
        Shutdown();
    }

    bool DiligentRenderBackend::Initialize(const RenderDesc &desc, NativeWindowHandle native_window) {
        impl_->desc = desc;

        if (!IsSupportedWindow(native_window, impl_->api)) {
            impl_->last_error = UnsupportedWindowMessage(impl_->api);
            return false;
        }

        const Diligent::SwapChainDesc swap_desc = MakeSwapChainDesc(desc);
        const Diligent::NativeWindow window = MakeNativeWindow(native_window);

        switch (impl_->api) {
            case DiligentRenderBackendApi::D3D11: {
#if PLATFORM_WIN32
                auto *factory = Diligent::LoadAndGetEngineFactoryD3D11();
                if (!factory) {
                    impl_->last_error = "Failed to load D3D11 factory";
                    return false;
                }

                Diligent::EngineD3D11CreateInfo ci;
                factory->CreateDeviceAndContextsD3D11(ci, &impl_->device, &impl_->immediate_context);
                if (!impl_->device || !impl_->immediate_context) {
                    impl_->last_error = "Failed to create D3D11 device/context";
                    return false;
                }

                const Diligent::FullScreenModeDesc fs_desc = {};
                factory->CreateSwapChainD3D11(impl_->device, impl_->immediate_context,
                                              swap_desc, fs_desc, window, &impl_->swap_chain);
                break;
#else
                impl_->last_error = "D3D11 backend is only available on Windows";
                return false;
#endif
            }

            case DiligentRenderBackendApi::D3D12: {
#if PLATFORM_WIN32
                auto *factory = Diligent::LoadAndGetEngineFactoryD3D12();
                if (!factory) {
                    impl_->last_error = "Failed to load D3D12 factory";
                    return false;
                }

                Diligent::EngineD3D12CreateInfo ci;
                factory->CreateDeviceAndContextsD3D12(ci, &impl_->device, &impl_->immediate_context);
                if (!impl_->device || !impl_->immediate_context) {
                    impl_->last_error = "Failed to create D3D12 device/context";
                    return false;
                }

                const Diligent::FullScreenModeDesc fs_desc = {};
                factory->CreateSwapChainD3D12(impl_->device, impl_->immediate_context,
                                              swap_desc, fs_desc, window, &impl_->swap_chain);
                break;
#else
                impl_->last_error = "D3D12 backend is only available on Windows";
                return false;
#endif
            }

            case DiligentRenderBackendApi::Vulkan: {
#if PLATFORM_WIN32 || PLATFORM_MACOS
                auto *factory = Diligent::LoadAndGetEngineFactoryVk();
                if (!factory) {
                    impl_->last_error = "Failed to load Vulkan factory";
                    return false;
                }

                const Diligent::Version vulkan_version = factory->GetVulkanVersion();
                if (vulkan_version.Major == 0) {
                    impl_->last_error = "Vulkan is not supported by the current platform/runtime";
                    return false;
                }

                Diligent::EngineVkCreateInfo ci;
                factory->CreateDeviceAndContextsVk(ci, &impl_->device, &impl_->immediate_context);
                if (!impl_->device || !impl_->immediate_context) {
                    impl_->last_error = "Failed to create Vulkan device/context";
                    return false;
                }

                factory->CreateSwapChainVk(impl_->device, impl_->immediate_context,
                                           swap_desc, window, &impl_->swap_chain);
                break;
#else
                impl_->last_error = "Vulkan backend is not available on this platform";
                return false;
#endif
            }

            case DiligentRenderBackendApi::Metal:
                impl_->last_error = "Metal backend is not implemented";
                return false;
        }

        if (!impl_->swap_chain) {
            impl_->last_error = "Failed to create swap chain";
            return false;
        }

        if (!CreateSharedConstantBuffers(*impl_)) {
            impl_->last_error = "Failed to create shared constant buffers";
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

        auto *rtv = impl_->swap_chain->GetCurrentBackBufferRTV();
        auto *dsv = impl_->swap_chain->GetDepthBufferDSV();

        if (!rtv) {
            return;
        }

        Diligent::ITextureView *rtvs[] = {rtv};
        impl_->immediate_context->SetRenderTargets(
            1, rtvs, dsv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const std::array<float, 4> rgba{clear_color.r, clear_color.g, clear_color.b, clear_color.a};
        impl_->immediate_context->ClearRenderTarget(
            rtv, rgba.data(), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (dsv) {
            impl_->immediate_context->ClearDepthStencil(
                dsv,
                Diligent::CLEAR_DEPTH_FLAG,
                1.f, 0,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    }

    void DiligentRenderBackend::EndFrame() {
        if (!impl_->swap_chain) {
            return;
        }
        impl_->immediate_context->Flush();
        impl_->swap_chain->Present(impl_->desc.vsync ? 1u : 0u);
    }

    void DiligentRenderBackend::Resize(int width, int height) {
        if (!impl_->swap_chain || width <= 0 || height <= 0) {
            return;
        }
        impl_->swap_chain->Resize(static_cast<Diligent::Uint32>(width),
                                  static_cast<Diligent::Uint32>(height));
    }

    void DiligentRenderBackend::Shutdown() {
        if (!impl_) {
            return;
        }
        impl_->immediate_context.Release();
        impl_->mesh_registry.clear();
        impl_->material_registry.clear();
        impl_->material_hash_cache.clear();
        impl_->per_frame_cb.Release();
        impl_->per_object_cb.Release();
        impl_->swap_chain.Release();
        impl_->device.Release();
    }

    MeshHandle DiligentRenderBackend::UploadMesh(const MeshDesc &desc) {
        if (!impl_->device || !desc.IsValid()) {
            return {};
        }

        if (desc.index_format != IndexFormat::UInt32) {
            impl_->last_error = "Unsupported mesh index format";
            return {};
        }

        DiligentMeshData data = UploadMeshData(impl_->device, desc);
        if (!data.vertex_buffer || !data.index_buffer) {
            impl_->last_error = "Failed to upload mesh buffers";
            return {};
        }

        const uint32_t id = impl_->next_mesh_id++;
        data.generation = impl_->mesh_generation++;
        impl_->mesh_registry[id] = std::move(data);
        return MeshHandle{.id = id, .generation = impl_->mesh_registry[id].generation};
    }

    void DiligentRenderBackend::DestroyMesh(MeshHandle handle) {
        if (!handle.IsValid()) {
            return;
        }

        const auto it = impl_->mesh_registry.find(handle.id);
        if (it == impl_->mesh_registry.end() || it->second.generation != handle.generation) {
            return;
        }

        impl_->mesh_registry.erase(it);
    }

    MaterialHandle DiligentRenderBackend::ResolveMaterial(const MaterialDesc &desc) {
        const auto it = impl_->material_hash_cache.find(desc.hash);
        if (it != impl_->material_hash_cache.end()) {
            return it->second;
        }

        DiligentMaterialData data = CreateMaterial(*impl_, desc);
        if (!data.pso) {
            impl_->last_error = "Failed to create PSO for material";
            return {};
        }

        const uint32_t id = impl_->next_material_id++;
        impl_->material_registry[id] = std::move(data);

        const MaterialHandle handle{id};
        impl_->material_hash_cache[desc.hash] = handle;
        return handle;
    }

    void DiligentRenderBackend::SetCamera(const CameraData &camera) {
        if (!impl_->per_frame_cb) {
            return;
        }
        PerFrameCB cb{camera.projection * camera.view};
        UpdateBuffer(impl_->immediate_context,
                     impl_->per_frame_cb,
                     &cb, sizeof(cb));
    }

    void DiligentRenderBackend::SubmitBatch(const RenderBatch &batch) {
        const auto mat_it = impl_->material_registry.find(batch.material.id);
        const auto msh_it = impl_->mesh_registry.find(batch.mesh.id);

        if (mat_it == impl_->material_registry.end() ||
            msh_it == impl_->mesh_registry.end() ||
            msh_it->second.generation != batch.mesh.generation) {
            return;
        }

        const DiligentMaterialData &mat = mat_it->second;
        const DiligentMeshData &msh = msh_it->second;

        if (!mat.pso || !mat.srb || !msh.vertex_buffer || !msh.index_buffer) {
            return;
        }

        impl_->immediate_context->SetPipelineState(mat.pso);

        const uint64_t vb_offset = 0;
        Diligent::IBuffer *vbs[] = {msh.vertex_buffer};
        impl_->immediate_context->SetVertexBuffers(
            0, 1, vbs, &vb_offset,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

        impl_->immediate_context->SetIndexBuffer(
            msh.index_buffer, 0,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        impl_->immediate_context->CommitShaderResources(
            mat.srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        for (const RenderInstance &inst: batch.instances) {
            PerObjectCB object_cb{inst.transform};
            UpdateBuffer(impl_->immediate_context,
                         impl_->per_object_cb,
                         &object_cb, sizeof(object_cb));

            Diligent::DrawIndexedAttribs draw;
            draw.IndexType = Diligent::VT_UINT32;
            draw.NumIndices = msh.index_count;
            draw.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
            impl_->immediate_context->DrawIndexed(draw);
        }
    }

    std::string_view DiligentRenderBackend::LastError() const {
        return impl_ ? impl_->last_error : "backend unavailable";
    }
} // namespace CoreEngine
