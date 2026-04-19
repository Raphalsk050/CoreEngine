#include "platform/diligent/diligent_render_backend.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "EngineFactoryD3D11.h"
#include "EngineFactoryD3D12.h"
#include "DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "core/render/primitives.h"
#include "core/render/render_batch.h"
#include "platform/diligent/builtin_shaders.h"

#include <glm/glm.hpp>

namespace CoreEngine {
    namespace {
        struct PerFrameCB {
            glm::mat4 view_proj;
        };

        struct PerObjectCB {
            glm::mat4 model;
        };

        struct DiligentMeshData {
            Diligent::RefCntAutoPtr<Diligent::IBuffer> vertex_buffer;
            Diligent::RefCntAutoPtr<Diligent::IBuffer> index_buffer;
            uint32_t index_count = 0;
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
        std::unordered_map<int, MeshHandle> primitive_cache;

        uint32_t next_mesh_id = 1;
        uint32_t next_material_id = 1;

        glm::mat4 view_proj{1.f};
    };

    namespace {
        bool IsSupportedWindow(NativeWindowHandle h) {
            return h.platform == NativeWindowPlatform::Win32 && h.IsValid();
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
            const void *data, uint32_t byte_size,
            Diligent::BIND_FLAGS flags, const char *name) {
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

        DiligentMeshData UploadMesh(Diligent::IRenderDevice *device,
                                    std::span<const Vertex> vertices,
                                    std::span<const uint16_t> indices) {
            DiligentMeshData m;
            m.index_count = static_cast<uint32_t>(indices.size());

            m.vertex_buffer = CreateGpuBuffer(
                device,
                vertices.data(),
                static_cast<uint32_t>(vertices.size_bytes()),
                Diligent::BIND_VERTEX_BUFFER,
                "VB");

            m.index_buffer = CreateGpuBuffer(
                device,
                indices.data(),
                static_cast<uint32_t>(indices.size_bytes()),
                Diligent::BIND_INDEX_BUFFER,
                "IB");

            return m;
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

            Diligent::LayoutElement layout[] = {
                {0, 0, 3, Diligent::VT_FLOAT32, false},
                {1, 0, 3, Diligent::VT_FLOAT32, false},
                {2, 0, 3, Diligent::VT_FLOAT32, false},
                {3, 0, 2, Diligent::VT_FLOAT32, false},
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
                mat.material_cbuffer = CreateConstantBuffer(
                    impl.device,
                    static_cast<uint32_t>(desc.properties_data.size()),
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

        if (!IsSupportedWindow(native_window)) {
            impl_->last_error = "Diligent backend requires a valid Win32 HWND";
            return false;
        }

        const Diligent::SwapChainDesc swap_desc = MakeSwapChainDesc(desc);
        const Diligent::FullScreenModeDesc fs_desc = {};
        Diligent::Win32NativeWindow win(native_window.window);

        switch (impl_->api) {
            case DiligentRenderBackendApi::D3D11: {
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
                factory->CreateSwapChainD3D11(impl_->device, impl_->immediate_context,
                                              swap_desc, fs_desc, win, &impl_->swap_chain);
                break;
            }
            case DiligentRenderBackendApi::D3D12: {
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
                factory->CreateSwapChainD3D12(impl_->device, impl_->immediate_context,
                                              swap_desc, fs_desc, win, &impl_->swap_chain);
                break;
            }
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
        impl_->per_frame_cb.Release();
        impl_->per_object_cb.Release();
        impl_->swap_chain.Release();
        impl_->device.Release();
    }

    std::string_view DiligentRenderBackend::LastError() const {
        return impl_ ? impl_->last_error : "backend unavailable";
    }

    MeshHandle DiligentRenderBackend::GetOrCreatePrimitive(PrimitiveType type) {
        const auto key = static_cast<int>(type);
        const auto it = impl_->primitive_cache.find(key);
        if (it != impl_->primitive_cache.end()) {
            return it->second;
        }

        const auto verts = Primitives::VerticesFor(type);
        const auto indices = Primitives::IndicesFor(type);
        MeshHandle handle = CreateMesh(verts, indices);
        impl_->primitive_cache[key] = handle;
        return handle;
    }

    MeshHandle DiligentRenderBackend::CreateMesh(std::span<const Vertex> vertices,
                                                 std::span<const uint16_t> indices) {
        if (!impl_->device || vertices.empty() || indices.empty()) {
            return {};
        }

        DiligentMeshData data = UploadMesh(impl_->device, vertices, indices);
        if (!data.vertex_buffer || !data.index_buffer) {
            impl_->last_error = "Failed to upload mesh buffers";
            return {};
        }

        const uint32_t id = impl_->next_mesh_id++;
        impl_->mesh_registry[id] = std::move(data);
        return MeshHandle{id};
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

        if (data.material_cbuffer && !data.properties_data.empty()) {
            UpdateBuffer(impl_->immediate_context,
                         data.material_cbuffer,
                         data.properties_data.data(),
                         static_cast<uint32_t>(data.properties_data.size()));
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
            msh_it == impl_->mesh_registry.end()) {
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
            draw.IndexType = Diligent::VT_UINT16;
            draw.NumIndices = msh.index_count;
            draw.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
            impl_->immediate_context->DrawIndexed(draw);
        }
    }
} // namespace CoreEngine
