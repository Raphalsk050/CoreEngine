#include "platform/diligent/diligent_render_backend.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <tsl/robin_map.h>

#include "EngineFactoryVk.h"

#if PLATFORM_WIN32 && D3D11_SUPPORTED
#include "EngineFactoryD3D11.h"
#endif

#if PLATFORM_WIN32 && D3D12_SUPPORTED
#include "EngineFactoryD3D12.h"
#endif
#include "DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "ImGuiImplSDL3.hpp"
#include "TextureLoader.h"
#include "TextureUtilities.h"
#include "platform/diligent/builtin_shaders.h"
#include "core/render/render_batch.h"
#include "core/render/vertex.h"

#include "core/math/math.h"

namespace CoreEngine {
    namespace {
        struct PerFrameCB {
            Math::Mat4 view_proj;
            Math::Vec4 frame_clock;
        };

        struct PerObjectCB {
            Math::Mat4 model;
        };

        struct DepthVisualizationCB {
            Math::Vec4 params;
        };

        struct DiligentUniformBinding {
            std::string name;
            ShaderStage stages = ShaderStage::Pixel;
            uint32_t byte_size = 0;
            Diligent::RefCntAutoPtr<Diligent::IBuffer> buffer;
            Diligent::IShaderResourceVariable *variable = nullptr;
        };

        struct DiligentTextureBinding {
            std::string name;
            std::string sampler_name;
            TextureHandle texture;
            ShaderStage stages = ShaderStage::Pixel;
            Diligent::IShaderResourceVariable *texture_variable = nullptr;
            Diligent::IShaderResourceVariable *sampler_variable = nullptr;
            uint64_t bound_revision = 0;
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
            Diligent::RefCntAutoPtr<Diligent::ISampler> sampler;
            std::vector<DiligentUniformBinding> uniforms;
            std::vector<DiligentTextureBinding> textures;
            std::vector<uint8_t> properties_data;
            uint32_t generation = 0;
        };

        struct DiligentShaderProgramData {
            Diligent::RefCntAutoPtr<Diligent::IPipelineState> pso;
            Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
            Diligent::RefCntAutoPtr<Diligent::ISampler> sampler;
            std::vector<DiligentUniformBinding> uniforms;
            std::vector<DiligentTextureBinding> textures;
            uint32_t generation = 0;
        };

        struct DiligentTextureData {
            Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
            Diligent::RefCntAutoPtr<Diligent::ITextureView> texture_view;
            TextureLoadState state = TextureLoadState::Invalid;
            std::string error_message;
            uint64_t revision = 0;
            uint32_t generation = 0;
        };

        struct DiligentTextureSnapshot {
            Diligent::RefCntAutoPtr<Diligent::ITextureView> texture_view;
            TextureLoadState state = TextureLoadState::Invalid;
            uint64_t revision = 0;
        };

        struct DiligentFrameBufferData {
            Diligent::RefCntAutoPtr<Diligent::ITexture> color_texture;
            Diligent::RefCntAutoPtr<Diligent::ITexture> depth_texture;
            Diligent::RefCntAutoPtr<Diligent::ITextureView> color_rtv;
            Diligent::RefCntAutoPtr<Diligent::ITextureView> color_srv;
            Diligent::RefCntAutoPtr<Diligent::ITextureView> depth_dsv;
            Diligent::RefCntAutoPtr<Diligent::ITextureView> depth_srv;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t generation = 0;
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
        std::unique_ptr<Diligent::ImGuiImplSDL3> imgui;

        Diligent::RefCntAutoPtr<Diligent::IBuffer> per_frame_cb;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> per_object_cb;

        tsl::robin_map<uint32_t, DiligentMeshData> mesh_registry;
        tsl::robin_map<uint32_t, DiligentMaterialData> material_registry;
        tsl::robin_map<uint32_t, DiligentShaderProgramData> shader_program_registry;
        tsl::robin_map<uint32_t, DiligentFrameBufferData> frame_buffer_registry;
        tsl::robin_map<uint32_t, DiligentTextureData> texture_registry;
        tsl::robin_map<uint64_t, MaterialHandle> material_hash_cache;
        mutable std::mutex texture_registry_mutex;
        std::vector<std::jthread> texture_load_workers;

        Diligent::RefCntAutoPtr<Diligent::IPipelineState> composite_pso;
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> composite_srb;
        Diligent::RefCntAutoPtr<Diligent::ISampler> composite_sampler;
        Diligent::IShaderResourceVariable *composite_scene_color_var = nullptr;
        Diligent::IShaderResourceVariable *composite_scene_sampler_var = nullptr;
        FrameBufferHandle active_frame_buffer{};
        ShaderProgramHandle active_shader_program{};
        ShaderProgramHandle depth_visualization_program{};

        uint32_t next_texture_id_ = 1;
        uint32_t texture_generation_ = 1;
        uint32_t next_mesh_id = 1;
        uint32_t next_material_id = 1;
        uint32_t next_shader_program_id = 1;
        uint32_t next_frame_buffer_id = 1;
        uint32_t mesh_generation = 1;
        uint32_t material_generation = 1;
        uint32_t shader_program_generation = 1;
        uint32_t frame_buffer_generation = 1;
    };

    namespace {
        bool IsSupportedWindow(NativeWindowHandle h, DiligentRenderBackendApi api) {
            if (!h.IsValid()) {
                return false;
            }

            switch (api) {
                case DiligentRenderBackendApi::D3D11:
#if PLATFORM_WIN32 && D3D11_SUPPORTED
                    return h.platform == NativeWindowPlatform::Win32;
#else
                    return false;
#endif

                case DiligentRenderBackendApi::D3D12:
#if PLATFORM_WIN32 && D3D12_SUPPORTED
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
                    return "Diligent D3D backend requires a valid Win32 HWND";
                case DiligentRenderBackendApi::D3D12:
                    return "Diligent D3D12 backend requires a valid Win32 HWND";
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

        Diligent::TEXTURE_FORMAT ToDiligentTextureFormat(TextureFormat format) {
            switch (format) {
                case TextureFormat::Auto:
                    return Diligent::TEX_FORMAT_UNKNOWN;
                case TextureFormat::RGBA8Unorm:
                    return Diligent::TEX_FORMAT_RGBA8_UNORM;
                case TextureFormat::RGBA8UnormSrgb:
                    return Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
            }
            return Diligent::TEX_FORMAT_UNKNOWN;
        }

        Diligent::TextureLoadInfo MakeTextureLoadInfo(const TextureLoadDesc &desc) {
            Diligent::TextureLoadInfo info;
            info.Name = desc.path.c_str();
            info.Usage = Diligent::USAGE_IMMUTABLE;
            info.BindFlags = Diligent::BIND_SHADER_RESOURCE;
            info.Format = ToDiligentTextureFormat(desc.format);
            info.GenerateMips = desc.generate_mipmaps;
            info.FlipVertically = desc.flip_vertically;
            info.PermultiplyAlpha = desc.premultiply_alpha;
            info.IsSRGB = desc.format == TextureFormat::RGBA8UnormSrgb;
            return info;
        }

        DiligentTextureData LoadTextureFromFile(Diligent::IRenderDevice *device, const TextureLoadDesc &desc) {
            DiligentTextureData data;
            const Diligent::TextureLoadInfo info = MakeTextureLoadInfo(desc);

            Diligent::CreateTextureFromFile(
                desc.path.c_str(),
                info,
                device,
                &data.texture);

            if (!data.texture) {
                data.state = TextureLoadState::Failed;
                data.error_message = "Failed to load texture from file: " + desc.path;
                return data;
            }

            data.texture_view = data.texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
            if (!data.texture_view) {
                data.state = TextureLoadState::Failed;
                data.error_message = "Failed to create texture shader resource view";
                return data;
            }

            data.state = TextureLoadState::Ready;
            data.revision = 1;
            return data;
        }

        DiligentTextureData CreateFallbackTexture(Diligent::IRenderDevice *device) {
            DiligentTextureData data;
            const std::array<std::uint8_t, 4> pixel{255u, 255u, 255u, 255u};

            Diligent::TextureDesc desc;
            desc.Name = "AsyncTextureFallback";
            desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
            desc.Width = 1;
            desc.Height = 1;
            desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
            desc.MipLevels = 1;
            desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
            desc.Usage = Diligent::USAGE_IMMUTABLE;

            Diligent::TextureSubResData sub_resource{pixel.data(), 4};
            Diligent::TextureData initial_data{&sub_resource, 1};
            device->CreateTexture(desc, &initial_data, &data.texture);

            if (!data.texture) {
                data.state = TextureLoadState::Failed;
                data.error_message = "Failed to create async texture fallback";
                return data;
            }

            data.texture_view = data.texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
            if (!data.texture_view) {
                data.state = TextureLoadState::Failed;
                data.error_message = "Failed to create async texture fallback view";
                return data;
            }

            data.state = TextureLoadState::Pending;
            data.revision = 1;
            return data;
        }

        DiligentTextureSnapshot GetTextureSnapshot(const DiligentRenderBackend::Impl &impl, TextureHandle handle) {
            std::lock_guard lock{impl.texture_registry_mutex};
            const auto it = impl.texture_registry.find(handle.id);
            if (it == impl.texture_registry.end() || it.value().generation != handle.generation) {
                return {};
            }

            return DiligentTextureSnapshot{
                .texture_view = it.value().texture_view,
                .state = it.value().state,
                .revision = it.value().revision,
            };
        }

        Diligent::SHADER_TYPE ToDiligentShaderStages(ShaderStage stage) {
            int result = Diligent::SHADER_TYPE_UNKNOWN;
            if (HasShaderStage(stage, ShaderStage::Vertex)) {
                result |= Diligent::SHADER_TYPE_VERTEX;
            }
            if (HasShaderStage(stage, ShaderStage::Pixel)) {
                result |= Diligent::SHADER_TYPE_PIXEL;
            }
            return static_cast<Diligent::SHADER_TYPE>(result);
        }

        Diligent::SHADER_TYPE PrimaryShaderStage(ShaderStage stage) {
            if (HasShaderStage(stage, ShaderStage::Pixel)) {
                return Diligent::SHADER_TYPE_PIXEL;
            }

            return Diligent::SHADER_TYPE_VERTEX;
        }

        Diligent::IShaderResourceVariable *FindSrbVariable(Diligent::IShaderResourceBinding *srb,
                                                           ShaderStage stages,
                                                           const std::string &name) {
            if (srb == nullptr || name.empty()) {
                return nullptr;
            }

            if (HasShaderStage(stages, ShaderStage::Pixel)) {
                if (auto *variable = srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, name.c_str())) {
                    return variable;
                }
            }

            if (HasShaderStage(stages, ShaderStage::Vertex)) {
                return srb->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, name.c_str());
            }

            return nullptr;
        }

        Diligent::IShaderResourceVariable *FindStaticVariable(Diligent::IPipelineState *pso,
                                                              ShaderStage stages,
                                                              const std::string &name) {
            if (pso == nullptr || name.empty()) {
                return nullptr;
            }

            if (HasShaderStage(stages, ShaderStage::Pixel)) {
                if (auto *variable = pso->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, name.c_str())) {
                    return variable;
                }
            }

            if (HasShaderStage(stages, ShaderStage::Vertex)) {
                return pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, name.c_str());
            }

            return nullptr;
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
            std::vector<ShaderUniformData> uniforms = desc.uniforms;
            if (uniforms.empty() && !desc.properties_data.empty()) {
                uniforms.push_back(MakeShaderUniformData(
                    "PerMaterial",
                    ShaderStage::Pixel,
                    std::span<const uint8_t>(desc.properties_data)));
            }

            auto vs = CompileShader(impl.device, Diligent::SHADER_TYPE_VERTEX,
                                    desc.vertex_shader_source.c_str(), "VS");
            auto ps = CompileShader(impl.device, Diligent::SHADER_TYPE_PIXEL,
                                    desc.pixel_shader_source.c_str(), "PS");

            // Explicit offsets keep the vertex contract stable if math type packing changes.
            constexpr Diligent::Uint32 kStride = sizeof(StaticMeshVertex);

            Diligent::LayoutElement layout[] = {
                // InputIndex, BufferSlot, NumComponents, ValueType, IsNormalized,
                //   RelativeOffset, Stride
                {
                    0, 0, 3, Diligent::VT_FLOAT32, false,
                    static_cast<Diligent::Uint32>(offsetof(StaticMeshVertex, position)), kStride
                },
                {
                    1, 0, 3, Diligent::VT_FLOAT32, false,
                    static_cast<Diligent::Uint32>(offsetof(StaticMeshVertex, normal)), kStride
                },
                {
                    2, 0, 3, Diligent::VT_FLOAT32, false,
                    static_cast<Diligent::Uint32>(offsetof(StaticMeshVertex, color)), kStride
                },
                {
                    3, 0, 2, Diligent::VT_FLOAT32, false,
                    static_cast<Diligent::Uint32>(offsetof(StaticMeshVertex, uv)), kStride
                },
                {
                    4, 0, 4, Diligent::VT_FLOAT32, false,
                    static_cast<Diligent::Uint32>(offsetof(StaticMeshVertex, custom0)), kStride
                },
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
            pci.GraphicsPipeline.InputLayout.NumElements = 5;
            pci.pVS = vs;
            pci.pPS = ps;

            std::vector<Diligent::ShaderResourceVariableDesc> vars;
            vars.reserve(2u + uniforms.size() + desc.bindings.size() * 2u);
            vars.push_back({
                Diligent::SHADER_TYPE_VERTEX, "PerFrame",
                Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC
            });
            vars.push_back({
                Diligent::SHADER_TYPE_VERTEX, "PerObject",
                Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC
            });

            for (const ShaderUniformData &uniform: uniforms) {
                if (uniform.IsValid()) {
                    vars.push_back({
                        ToDiligentShaderStages(uniform.stages), uniform.name.c_str(),
                        Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC
                    });
                }
            }

            for (const ShaderBindingDesc &binding: desc.bindings) {
                if (!binding.IsValid() || binding.type != ShaderBindingType::Texture) {
                    continue;
                }

                vars.push_back({
                    ToDiligentShaderStages(binding.stages), binding.name.c_str(),
                    Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC
                });

                if (!binding.sampler_name.empty()) {
                    vars.push_back({
                        ToDiligentShaderStages(binding.stages), binding.sampler_name.c_str(),
                        Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC
                    });
                }
            }

            pci.PSODesc.ResourceLayout.Variables = vars.data();
            pci.PSODesc.ResourceLayout.NumVariables = static_cast<Diligent::Uint32>(vars.size());

            DiligentMaterialData mat;
            impl.device->CreateGraphicsPipelineState(pci, &mat.pso);

            if (!mat.pso) {
                return mat;
            }

            if (auto *per_frame = mat.pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "PerFrame")) {
                per_frame->Set(impl.per_frame_cb);
            }

            if (auto *per_object = mat.pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "PerObject")) {
                per_object->Set(impl.per_object_cb);
            }

            for (const ShaderUniformData &uniform: uniforms) {
                if (!uniform.IsValid()) {
                    continue;
                }

                DiligentUniformBinding binding;
                binding.name = uniform.name;
                binding.stages = uniform.stages;
                binding.byte_size = static_cast<uint32_t>(uniform.data.size());
                binding.buffer = CreateImmutableConstantBuffer(
                    impl.device,
                    uniform.data,
                    uniform.name.c_str());

                binding.variable = FindStaticVariable(mat.pso, uniform.stages, uniform.name);
                if (binding.buffer && binding.variable != nullptr) {
                    binding.variable->Set(binding.buffer);
                    if (uniform.name == "PerMaterial") {
                        mat.material_cbuffer = binding.buffer;
                    }
                }

                mat.uniforms.push_back(std::move(binding));
            }

            bool has_material_textures = false;
            for (const ShaderTextureData &texture: desc.textures) {
                if (!texture.name.empty() && !texture.texture.IsValid()) {
                    impl.last_error = "Material references an invalid texture handle";
                    return {};
                }

                if (texture.IsValid()) {
                    has_material_textures = true;
                    break;
                }
            }

            if (has_material_textures) {
                Diligent::SamplerDesc sampler;
                sampler.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
                sampler.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
                sampler.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;
                impl.device->CreateSampler(sampler, &mat.sampler);

                if (!mat.sampler) {
                    impl.last_error = "Failed to create material texture sampler";
                    return {};
                }
            }

            mat.properties_data = desc.properties_data;
            mat.pso->CreateShaderResourceBinding(&mat.srb, true);

            if (!mat.srb) {
                impl.last_error = "Failed to create material shader resource binding";
                return {};
            }

            for (const ShaderTextureData &texture: desc.textures) {
                if (!texture.IsValid()) {
                    continue;
                }

                const DiligentTextureSnapshot snapshot = GetTextureSnapshot(impl, texture.texture);
                if (!snapshot.texture_view) {
                    impl.last_error = "Material references an invalid texture handle";
                    return {};
                }

                DiligentTextureBinding binding;
                binding.name = texture.name;
                binding.sampler_name = texture.sampler_name;
                binding.texture = texture.texture;
                binding.stages = texture.stages;
                binding.texture_variable = FindSrbVariable(mat.srb, texture.stages, texture.name);
                binding.sampler_variable = FindSrbVariable(mat.srb, texture.stages, texture.sampler_name);

                if (binding.texture_variable == nullptr ||
                    (!binding.sampler_name.empty() && binding.sampler_variable == nullptr)) {
                    impl.last_error = "Failed to resolve material texture shader variable";
                    return {};
                }

                binding.texture_variable->Set(snapshot.texture_view);
                if (binding.sampler_variable != nullptr) {
                    binding.sampler_variable->Set(mat.sampler);
                }
                binding.bound_revision = snapshot.revision;

                mat.textures.push_back(std::move(binding));
            }

            return mat;
        }

        Diligent::TEXTURE_FORMAT ResolveFrameBufferColorFormat(FrameBufferFormat format,
                                                               Diligent::TEXTURE_FORMAT swap_chain_format) {
            switch (format) {
                case FrameBufferFormat::SwapChainColor:
                    return swap_chain_format;
                case FrameBufferFormat::RGBA8Unorm:
                    return Diligent::TEX_FORMAT_RGBA8_UNORM;
                case FrameBufferFormat::RGBA16Float:
                    return Diligent::TEX_FORMAT_RGBA16_FLOAT;
                case FrameBufferFormat::R32Float:
                    return Diligent::TEX_FORMAT_R32_FLOAT;
                case FrameBufferFormat::SwapChainDepth:
                case FrameBufferFormat::Depth32Float:
                    return Diligent::TEX_FORMAT_UNKNOWN;
            }

            return Diligent::TEX_FORMAT_UNKNOWN;
        }

        Diligent::TEXTURE_FORMAT ResolveFrameBufferDepthFormat(FrameBufferFormat format,
                                                               Diligent::TEXTURE_FORMAT swap_chain_format) {
            switch (format) {
                case FrameBufferFormat::SwapChainDepth:
                    return swap_chain_format;
                case FrameBufferFormat::Depth32Float:
                    return Diligent::TEX_FORMAT_D32_FLOAT;
                case FrameBufferFormat::SwapChainColor:
                case FrameBufferFormat::RGBA8Unorm:
                case FrameBufferFormat::RGBA16Float:
                case FrameBufferFormat::R32Float:
                    return Diligent::TEX_FORMAT_UNKNOWN;
            }

            return Diligent::TEX_FORMAT_UNKNOWN;
        }

        DiligentFrameBufferData CreateFrameBufferData(
            Diligent::IRenderDevice *device,
            const FrameBufferDesc &desc,
            Diligent::TEXTURE_FORMAT swap_chain_color_format,
            Diligent::TEXTURE_FORMAT swap_chain_depth_format) {
            DiligentFrameBufferData frame_buffer;
            frame_buffer.width = static_cast<uint32_t>(desc.width);
            frame_buffer.height = static_cast<uint32_t>(desc.height);

            if (desc.has_color) {
                const Diligent::TEXTURE_FORMAT color_format =
                        ResolveFrameBufferColorFormat(desc.color_format, swap_chain_color_format);
                if (color_format == Diligent::TEX_FORMAT_UNKNOWN) {
                    return frame_buffer;
                }

                Diligent::TextureDesc color_desc;
                color_desc.Name = "FrameBufferColor";
                color_desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
                color_desc.Width = frame_buffer.width;
                color_desc.Height = frame_buffer.height;
                color_desc.Format = color_format;
                color_desc.BindFlags = desc.sample_color
                                           ? Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE
                                           : Diligent::BIND_RENDER_TARGET;

                device->CreateTexture(color_desc, nullptr, &frame_buffer.color_texture);
                if (frame_buffer.color_texture) {
                    frame_buffer.color_rtv =
                            frame_buffer.color_texture->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);

                    if (desc.sample_color) {
                        frame_buffer.color_srv =
                                frame_buffer.color_texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
                    }
                }
            }

            if (desc.has_depth) {
                const Diligent::TEXTURE_FORMAT depth_format =
                        ResolveFrameBufferDepthFormat(desc.depth_format, swap_chain_depth_format);
                if (depth_format == Diligent::TEX_FORMAT_UNKNOWN) {
                    return frame_buffer;
                }

                Diligent::TextureDesc depth_desc;
                depth_desc.Name = "FrameBufferDepth";
                depth_desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
                depth_desc.Width = frame_buffer.width;
                depth_desc.Height = frame_buffer.height;
                depth_desc.Format = depth_format;
                depth_desc.BindFlags = desc.sample_depth
                                           ? Diligent::BIND_DEPTH_STENCIL | Diligent::BIND_SHADER_RESOURCE
                                           : Diligent::BIND_DEPTH_STENCIL;

                device->CreateTexture(depth_desc, nullptr, &frame_buffer.depth_texture);
                if (frame_buffer.depth_texture) {
                    frame_buffer.depth_dsv =
                            frame_buffer.depth_texture->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);

                    if (desc.sample_depth) {
                        frame_buffer.depth_srv =
                                frame_buffer.depth_texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
                    }
                }
            }

            return frame_buffer;
        }

        bool CreateCompositePipeline(DiligentRenderBackend::Impl &impl) {
            auto vs = CompileShader(impl.device, Diligent::SHADER_TYPE_VERTEX,
                                    BuiltinShaders::kCompositeVS, "CompositeVS");
            auto ps = CompileShader(impl.device, Diligent::SHADER_TYPE_PIXEL,
                                    BuiltinShaders::kCompositePS, "CompositePS");

            if (!vs || !ps) {
                return false;
            }

            Diligent::GraphicsPipelineStateCreateInfo pci;
            pci.PSODesc.Name = "FramebufferComposite";
            pci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
            pci.GraphicsPipeline.NumRenderTargets = 1;
            pci.GraphicsPipeline.RTVFormats[0] = impl.swap_chain->GetDesc().ColorBufferFormat;
            pci.GraphicsPipeline.DSVFormat = impl.swap_chain->GetDesc().DepthBufferFormat;
            pci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            pci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
            pci.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;
            pci.pVS = vs;
            pci.pPS = ps;

            Diligent::ShaderResourceVariableDesc vars[] = {
                {Diligent::SHADER_TYPE_PIXEL, "g_SceneColor", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {Diligent::SHADER_TYPE_PIXEL, "g_SceneColor_sampler", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            };
            pci.PSODesc.ResourceLayout.Variables = vars;
            pci.PSODesc.ResourceLayout.NumVariables = 2;

            Diligent::SamplerDesc sampler;
            sampler.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
            sampler.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
            sampler.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;

            impl.device->CreateGraphicsPipelineState(pci, &impl.composite_pso);
            if (!impl.composite_pso) {
                return false;
            }

            impl.device->CreateSampler(sampler, &impl.composite_sampler);
            if (!impl.composite_sampler) {
                return false;
            }

            impl.composite_pso->CreateShaderResourceBinding(&impl.composite_srb, true);
            if (!impl.composite_srb) {
                return false;
            }

            impl.composite_scene_color_var =
                    impl.composite_srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_SceneColor");
            impl.composite_scene_sampler_var =
                    impl.composite_srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_SceneColor_sampler");

            if (impl.composite_scene_sampler_var != nullptr) {
                impl.composite_scene_sampler_var->Set(impl.composite_sampler);
            }

            return impl.composite_scene_color_var != nullptr && impl.composite_scene_sampler_var != nullptr;
        }

        DiligentShaderProgramData CreateShaderProgramData(DiligentRenderBackend::Impl &impl,
                                                          const ShaderProgramDesc &desc) {
            DiligentShaderProgramData program;
            if (!desc.IsValid()) {
                return program;
            }

            const char *vertex_source = desc.vertex_shader_source.empty()
                                            ? BuiltinShaders::kCompositeVS
                                            : desc.vertex_shader_source.c_str();

            auto vs = CompileShader(impl.device, Diligent::SHADER_TYPE_VERTEX,
                                    vertex_source, "ShaderProgramVS");
            auto ps = CompileShader(impl.device, Diligent::SHADER_TYPE_PIXEL,
                                    desc.pixel_shader_source.c_str(), "ShaderProgramPS");

            if (!vs || !ps) {
                return program;
            }

            const Diligent::SwapChainDesc swap_desc = impl.swap_chain->GetDesc();
            const Diligent::TEXTURE_FORMAT color_format =
                    ResolveFrameBufferColorFormat(desc.color_format, swap_desc.ColorBufferFormat);
            const Diligent::TEXTURE_FORMAT depth_format =
                    desc.depth_test
                        ? ResolveFrameBufferDepthFormat(desc.depth_format, swap_desc.DepthBufferFormat)
                        : Diligent::TEX_FORMAT_UNKNOWN;

            if (color_format == Diligent::TEX_FORMAT_UNKNOWN ||
                (desc.depth_test && depth_format == Diligent::TEX_FORMAT_UNKNOWN)) {
                return program;
            }

            std::vector<Diligent::ShaderResourceVariableDesc> vars;
            vars.reserve(desc.bindings.size() * 2u);

            for (const ShaderBindingDesc &binding: desc.bindings) {
                if (!binding.IsValid()) {
                    continue;
                }

                vars.push_back({
                    ToDiligentShaderStages(binding.stages), binding.name.c_str(),
                    Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC
                });

                if (binding.type == ShaderBindingType::Texture && !binding.sampler_name.empty()) {
                    vars.push_back({
                        ToDiligentShaderStages(binding.stages), binding.sampler_name.c_str(),
                        Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC
                    });
                }
            }

            Diligent::GraphicsPipelineStateCreateInfo pci;
            pci.PSODesc.Name = "ShaderProgram";
            pci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
            pci.GraphicsPipeline.NumRenderTargets = 1;
            pci.GraphicsPipeline.RTVFormats[0] = color_format;
            pci.GraphicsPipeline.DSVFormat = depth_format;
            pci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            pci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
            pci.GraphicsPipeline.DepthStencilDesc.DepthEnable = desc.depth_test;
            pci.PSODesc.ResourceLayout.Variables = vars.data();
            pci.PSODesc.ResourceLayout.NumVariables = static_cast<Diligent::Uint32>(vars.size());
            pci.pVS = vs;
            pci.pPS = ps;

            impl.device->CreateGraphicsPipelineState(pci, &program.pso);
            if (!program.pso) {
                return {};
            }

            Diligent::SamplerDesc sampler;
            sampler.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
            sampler.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
            sampler.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;
            impl.device->CreateSampler(sampler, &program.sampler);

            program.pso->CreateShaderResourceBinding(&program.srb, true);
            if (!program.srb) {
                return {};
            }

            for (const ShaderBindingDesc &binding: desc.bindings) {
                if (!binding.IsValid()) {
                    continue;
                }

                if (binding.type == ShaderBindingType::UniformBuffer) {
                    DiligentUniformBinding uniform;
                    uniform.name = binding.name;
                    uniform.stages = binding.stages;
                    uniform.byte_size = AlignConstantBufferSize(binding.byte_size);
                    uniform.buffer = CreateConstantBuffer(impl.device, uniform.byte_size, binding.name.c_str());
                    uniform.variable = FindSrbVariable(program.srb, binding.stages, binding.name);

                    if (!uniform.buffer || uniform.variable == nullptr) {
                        return {};
                    }

                    uniform.variable->Set(uniform.buffer);
                    program.uniforms.push_back(std::move(uniform));
                    continue;
                }

                DiligentTextureBinding texture;
                texture.name = binding.name;
                texture.sampler_name = binding.sampler_name;
                texture.stages = binding.stages;
                texture.texture_variable = FindSrbVariable(program.srb, binding.stages, binding.name);
                texture.sampler_variable = FindSrbVariable(program.srb, binding.stages, binding.sampler_name);

                if (texture.texture_variable == nullptr ||
                    (!texture.sampler_name.empty() && (texture.sampler_variable == nullptr || !program.sampler))) {
                    return {};
                }

                if (texture.sampler_variable != nullptr) {
                    texture.sampler_variable->Set(program.sampler);
                }

                program.textures.push_back(std::move(texture));
            }

            return program;
        }

        ShaderProgramDesc MakeDepthVisualizationProgramDesc() {
            return ShaderProgramDesc{
                .vertex_shader_source = BuiltinShaders::kCompositeVS,
                .pixel_shader_source = BuiltinShaders::kDepthVisualizationPS,
                .bindings = {
                    ShaderBindingDesc::Texture("g_DepthTexture"),
                    ShaderBindingDesc::UniformBuffer(
                        "DepthVisualization",
                        sizeof(DepthVisualizationCB),
                        ShaderBindingScope::Pass,
                        ShaderStage::Pixel),
                },
            };
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

    TextureHandle DiligentRenderBackend::LoadTexture2D(const TextureLoadDesc &desc) {
        if (!impl_ || !impl_->device || !desc.IsValid()) {
            if (impl_) {
                impl_->last_error = "Invalid texture load request";
            }
            return {};
        }

        DiligentTextureData data = LoadTextureFromFile(impl_->device, desc);
        if (!data.texture_view) {
            impl_->last_error = data.error_message.empty() ? "Failed to load texture" : data.error_message;
            return {};
        }

        std::lock_guard lock{impl_->texture_registry_mutex};
        const uint32_t id = impl_->next_texture_id_++;
        data.generation = impl_->texture_generation_++;
        impl_->texture_registry[id] = std::move(data);

        return TextureHandle{.id = id, .generation = impl_->texture_registry[id].generation};
    }

    TextureHandle DiligentRenderBackend::LoadTexture2DAsync(const TextureLoadDesc &desc) {
        if (!impl_ || !impl_->device || !desc.IsValid()) {
            if (impl_) {
                impl_->last_error = "Invalid async texture load request";
            }
            return {};
        }

        DiligentTextureData fallback = CreateFallbackTexture(impl_->device);
        if (!fallback.texture_view) {
            impl_->last_error = fallback.error_message.empty()
                                    ? "Failed to create async texture fallback"
                                    : fallback.error_message;
            return {};
        }

        TextureHandle handle;
        {
            std::lock_guard lock{impl_->texture_registry_mutex};
            handle = TextureHandle{
                .id = impl_->next_texture_id_++,
                .generation = impl_->texture_generation_++,
            };
            fallback.generation = handle.generation;
            impl_->texture_registry[handle.id] = std::move(fallback);
        }

        impl_->texture_load_workers.emplace_back([impl = impl_.get(), handle, desc](std::stop_token stop_token) {
            try {
                DiligentTextureData loaded = LoadTextureFromFile(impl->device, desc);
                if (stop_token.stop_requested()) {
                    return;
                }

                std::lock_guard lock{impl->texture_registry_mutex};
                const auto it = impl->texture_registry.find(handle.id);
                if (it == impl->texture_registry.end() || it.value().generation != handle.generation) {
                    return;
                }

                if (!loaded.texture_view) {
                    it.value().state = TextureLoadState::Failed;
                    it.value().error_message = loaded.error_message;
                    return;
                }

                it.value().texture = std::move(loaded.texture);
                it.value().texture_view = std::move(loaded.texture_view);
                it.value().state = TextureLoadState::Ready;
                ++it.value().revision;
            } catch (const std::exception &ex) {
                std::lock_guard lock{impl->texture_registry_mutex};
                const auto it = impl->texture_registry.find(handle.id);
                if (it != impl->texture_registry.end() && it.value().generation == handle.generation) {
                    it.value().state = TextureLoadState::Failed;
                    it.value().error_message = ex.what();
                }
            }
        });

        return handle;
    }

    TextureLoadState DiligentRenderBackend::GetTextureLoadState(TextureHandle handle) const {
        if (!impl_ || !handle.IsValid()) {
            return TextureLoadState::Invalid;
        }

        return GetTextureSnapshot(*impl_, handle).state;
    }

    void DiligentRenderBackend::DestroyTexture(TextureHandle handle) {
        if (!impl_ || !handle.IsValid()) {
            return;
        }

        std::lock_guard lock{impl_->texture_registry_mutex};
        const auto it = impl_->texture_registry.find(handle.id);
        if (it == impl_->texture_registry.end() || it.value().generation != handle.generation) {
            return;
        }

        impl_->texture_registry.erase(it);
    }

    void DiligentRenderBackend::BindShaderTexture(std::string_view name, TextureHandle handle) {
        if (!impl_ || !handle.IsValid() || !impl_->active_shader_program.IsValid()) {
            return;
        }

        const DiligentTextureSnapshot snapshot = GetTextureSnapshot(*impl_, handle);
        if (!snapshot.texture_view) {
            return;
        }

        auto program_it = impl_->shader_program_registry.find(impl_->active_shader_program.id);
        if (program_it == impl_->shader_program_registry.end() ||
            program_it.value().generation != impl_->active_shader_program.generation) {
            return;
        }

        DiligentShaderProgramData &program = program_it.value();
        for (DiligentTextureBinding &texture: program.textures) {
            if (std::string_view{texture.name} != name) {
                continue;
            }

            texture.texture_variable->Set(snapshot.texture_view);
            if (texture.sampler_variable != nullptr) {
                texture.sampler_variable->Set(program.sampler);
            }
            texture.bound_revision = snapshot.revision;
            return;
        }
    }

    void DiligentRenderBackend::RenderDepthToColor(FrameBufferDepthView source, FrameBufferHandle destination,
                                                   const DepthVisualizationDesc &desc) {
        if (!source.IsValid() || !destination.IsValid() || !impl_->depth_visualization_program.IsValid()) {
            return;
        }

        const DepthVisualizationCB cb{
            .params = Math::Vec4(
                desc.scale,
                desc.bias,
                desc.exponent,
                desc.invert ? 1.0f : 0.0f),
        };

        SetFrameBuffer(destination);
        UseShaderProgram(impl_->depth_visualization_program);
        BindShaderTexture("g_DepthTexture", source);
        BindShaderUniform(
            "DepthVisualization",
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t *>(&cb),
                sizeof(cb)));
        Draw(3u, 1u);
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
#if PLATFORM_WIN32 && D3D11_SUPPORTED
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
                impl_->last_error = "D3D11 backend is not available in this build";
                return false;
#endif
            }

            case DiligentRenderBackendApi::D3D12: {
#if PLATFORM_WIN32 && D3D12_SUPPORTED
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
                impl_->last_error = "D3D12 backend is not available in this build";
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

        if (!CreateCompositePipeline(*impl_)) {
            impl_->last_error = "Failed to create framebuffer composite pipeline";
            return false;
        }

        impl_->depth_visualization_program = CreateShaderProgram(MakeDepthVisualizationProgramDesc());
        if (!impl_->depth_visualization_program.IsValid()) {
            impl_->last_error = "Failed to create depth visualization shader program";
            return false;
        }

        if (desc.enable_imgui && native_window.platform_window != nullptr) {
            try {
                Diligent::ImGuiDiligentCreateInfo imgui_ci{impl_->device, impl_->swap_chain->GetDesc()};
                impl_->imgui = Diligent::ImGuiImplSDL3::Create(
                    imgui_ci,
                    static_cast<SDL_Window *>(native_window.platform_window));
            } catch (const std::exception &error) {
                impl_->last_error = error.what();
                impl_->imgui.reset();
            } catch (...) {
                impl_->last_error = "Failed to initialize ImGui";
                impl_->imgui.reset();
            }
        }

        return true;
    }

    void DiligentRenderBackend::BeginFrame() {
    }

    void DiligentRenderBackend::Clear(const RenderClearColor &clear_color) {
        if (!impl_->swap_chain || !impl_->immediate_context) {
            return;
        }

        Diligent::ITextureView *rtv = nullptr;
        Diligent::ITextureView *dsv = nullptr;

        if (impl_->active_frame_buffer.IsValid()) {
            const auto it = impl_->frame_buffer_registry.find(impl_->active_frame_buffer.id);
            if (it != impl_->frame_buffer_registry.end() &&
                it.value().generation == impl_->active_frame_buffer.generation) {
                rtv = it.value().color_rtv;
                dsv = it.value().depth_dsv;
            }
        } else {
            rtv = impl_->swap_chain->GetCurrentBackBufferRTV();
            dsv = impl_->swap_chain->GetDepthBufferDSV();
        }

        if (!rtv && !dsv) {
            return;
        }

        Diligent::ITextureView *rtvs[] = {rtv};
        const Diligent::Uint32 render_target_count = rtv != nullptr ? 1u : 0u;
        impl_->immediate_context->SetRenderTargets(
            render_target_count,
            rtv != nullptr ? rtvs : nullptr,
            dsv,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (rtv) {
            const std::array<float, 4> rgba{clear_color.r, clear_color.g, clear_color.b, clear_color.a};
            impl_->immediate_context->ClearRenderTarget(
                rtv, rgba.data(), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        if (dsv) {
            impl_->immediate_context->ClearDepthStencil(
                dsv,
                Diligent::CLEAR_DEPTH_FLAG,
                1.f, 0,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    }

    void DiligentRenderBackend::BeginImGuiFrame() {
        if (!impl_->imgui || !impl_->swap_chain) {
            return;
        }

        const Diligent::SwapChainDesc &swap_desc = impl_->swap_chain->GetDesc();
        impl_->imgui->NewFrame(swap_desc.Width, swap_desc.Height, swap_desc.PreTransform);
    }

    void DiligentRenderBackend::RenderImGui() {
        if (!impl_->imgui || !impl_->immediate_context) {
            return;
        }

        impl_->imgui->Render(impl_->immediate_context);
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
        for (std::jthread &worker: impl_->texture_load_workers) {
            worker.request_stop();
        }
        impl_->texture_load_workers.clear();
        {
            std::lock_guard lock{impl_->texture_registry_mutex};
            impl_->texture_registry.clear();
        }
        impl_->imgui.reset();
        impl_->active_frame_buffer = {};
        impl_->active_shader_program = {};
        impl_->depth_visualization_program = {};
        impl_->composite_scene_color_var = nullptr;
        impl_->composite_scene_sampler_var = nullptr;
        impl_->composite_sampler.Release();
        impl_->composite_srb.Release();
        impl_->composite_pso.Release();
        impl_->shader_program_registry.clear();
        impl_->frame_buffer_registry.clear();
        impl_->mesh_registry.clear();
        impl_->material_registry.clear();
        impl_->material_hash_cache.clear();
        impl_->immediate_context.Release();
        impl_->per_frame_cb.Release();
        impl_->per_object_cb.Release();
        impl_->swap_chain.Release();
        impl_->device.Release();
    }

    FrameBufferHandle DiligentRenderBackend::CreateFrameBuffer(const FrameBufferDesc &desc) {
        if (!impl_->device || !impl_->swap_chain || !desc.IsValid()) {
            return {};
        }

        const Diligent::SwapChainDesc swap_desc = impl_->swap_chain->GetDesc();
        DiligentFrameBufferData data = CreateFrameBufferData(
            impl_->device,
            desc,
            swap_desc.ColorBufferFormat,
            swap_desc.DepthBufferFormat);

        if ((desc.has_color && (!data.color_texture || !data.color_rtv)) ||
            (desc.has_depth && (!data.depth_texture || !data.depth_dsv))) {
            impl_->last_error = "Failed to create framebuffer textures";
            return {};
        }

        if (desc.sample_color && !data.color_srv) {
            impl_->last_error = "Failed to create framebuffer shader resource view";
            return {};
        }

        if (desc.sample_depth && !data.depth_srv) {
            impl_->last_error = "Failed to create framebuffer depth shader resource view";
            return {};
        }

        const uint32_t id = impl_->next_frame_buffer_id++;
        data.generation = impl_->frame_buffer_generation++;
        impl_->frame_buffer_registry[id] = std::move(data);
        return FrameBufferHandle{.id = id, .generation = impl_->frame_buffer_registry[id].generation};
    }

    void DiligentRenderBackend::DestroyFrameBuffer(FrameBufferHandle handle) {
        if (!handle.IsValid()) {
            return;
        }

        const auto it = impl_->frame_buffer_registry.find(handle.id);
        if (it == impl_->frame_buffer_registry.end() || it.value().generation != handle.generation) {
            return;
        }

        if (impl_->active_frame_buffer == handle) {
            impl_->active_frame_buffer = {};
        }

        impl_->frame_buffer_registry.erase(it);
    }

    void DiligentRenderBackend::SetFrameBuffer(FrameBufferHandle handle) {
        if (!impl_->immediate_context || !handle.IsValid()) {
            return;
        }

        const auto it = impl_->frame_buffer_registry.find(handle.id);
        if (it == impl_->frame_buffer_registry.end() || it.value().generation != handle.generation) {
            return;
        }

        Diligent::ITextureView *rtv = it.value().color_rtv.RawPtr();
        Diligent::ITextureView *rtvs[] = {rtv};
        const Diligent::Uint32 render_target_count = rtv != nullptr ? 1u : 0u;
        impl_->immediate_context->SetRenderTargets(
            render_target_count,
            rtv != nullptr ? rtvs : nullptr,
            it.value().depth_dsv,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        impl_->active_frame_buffer = handle;
    }

    void DiligentRenderBackend::SetSwapChainFrameBuffer() {
        if (!impl_->immediate_context || !impl_->swap_chain) {
            return;
        }

        Diligent::ITextureView *rtvs[] = {impl_->swap_chain->GetCurrentBackBufferRTV()};
        impl_->immediate_context->SetRenderTargets(
            1,
            rtvs,
            impl_->swap_chain->GetDepthBufferDSV(),
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        impl_->active_frame_buffer = {};
    }

    FrameBufferColorView DiligentRenderBackend::GetFrameBufferColorView(FrameBufferHandle handle) const {
        if (!handle.IsValid()) {
            return {};
        }

        const auto it = impl_->frame_buffer_registry.find(handle.id);
        if (it == impl_->frame_buffer_registry.end() || it.value().generation != handle.generation) {
            return {};
        }

        return FrameBufferColorView{
            .native_handle = reinterpret_cast<NativeFrameBufferColorView *>(it.value().color_srv.RawPtr())
        };
    }

    FrameBufferDepthView DiligentRenderBackend::GetFrameBufferDepthView(FrameBufferHandle handle) const {
        if (!handle.IsValid()) {
            return {};
        }

        const auto it = impl_->frame_buffer_registry.find(handle.id);
        if (it == impl_->frame_buffer_registry.end() || it.value().generation != handle.generation) {
            return {};
        }

        return FrameBufferDepthView{
            .native_handle = reinterpret_cast<NativeFrameBufferDepthView *>(it.value().depth_srv.RawPtr())
        };
    }

    void DiligentRenderBackend::CompositeFrameBuffer(FrameBufferHandle source) {
        if (!impl_->immediate_context || !impl_->composite_pso || !impl_->composite_srb ||
            impl_->composite_scene_color_var == nullptr) {
            return;
        }

        const auto it = impl_->frame_buffer_registry.find(source.id);
        if (it == impl_->frame_buffer_registry.end() || it.value().generation != source.generation ||
            !it.value().color_srv) {
            return;
        }

        SetSwapChainFrameBuffer();

        impl_->immediate_context->SetPipelineState(impl_->composite_pso);
        impl_->composite_scene_color_var->Set(it.value().color_srv);
        impl_->composite_scene_sampler_var->Set(impl_->composite_sampler);
        impl_->immediate_context->CommitShaderResources(
            impl_->composite_srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::DrawAttribs draw;
        draw.NumVertices = 3;
        draw.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
        impl_->immediate_context->Draw(draw);
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
        if (it == impl_->mesh_registry.end() || it.value().generation != handle.generation) {
            return;
        }

        impl_->mesh_registry.erase(it);
    }

    MaterialHandle DiligentRenderBackend::ResolveMaterial(const MaterialDesc &desc) {
        const auto it = impl_->material_hash_cache.find(desc.hash);
        if (it != impl_->material_hash_cache.end()) {
            return it.value();
        }

        DiligentMaterialData data = CreateMaterial(*impl_, desc);
        if (!data.pso) {
            if (impl_->last_error.empty()) {
                impl_->last_error = "Failed to create PSO for material";
            }
            return {};
        }

        const uint32_t id = impl_->next_material_id++;
        data.generation = impl_->material_generation++;
        impl_->material_registry[id] = std::move(data);

        const MaterialHandle handle{.id = id, .generation = impl_->material_registry[id].generation};
        impl_->material_hash_cache[desc.hash] = handle;
        return handle;
    }

    ShaderProgramHandle DiligentRenderBackend::CreateShaderProgram(const ShaderProgramDesc &desc) {
        if (!impl_->device || !impl_->swap_chain || !desc.IsValid()) {
            return {};
        }

        DiligentShaderProgramData data = CreateShaderProgramData(*impl_, desc);
        if (!data.pso || !data.srb) {
            impl_->last_error = "Failed to create shader program";
            return {};
        }

        const uint32_t id = impl_->next_shader_program_id++;
        data.generation = impl_->shader_program_generation++;
        impl_->shader_program_registry[id] = std::move(data);
        return ShaderProgramHandle{.id = id, .generation = impl_->shader_program_registry[id].generation};
    }

    void DiligentRenderBackend::DestroyShaderProgram(ShaderProgramHandle handle) {
        if (!handle.IsValid()) {
            return;
        }

        const auto it = impl_->shader_program_registry.find(handle.id);
        if (it == impl_->shader_program_registry.end() || it.value().generation != handle.generation) {
            return;
        }

        if (impl_->active_shader_program == handle) {
            impl_->active_shader_program = {};
        }

        impl_->shader_program_registry.erase(it);
    }

    void DiligentRenderBackend::UseShaderProgram(ShaderProgramHandle handle) {
        if (!handle.IsValid() || !impl_->immediate_context) {
            impl_->active_shader_program = {};
            return;
        }

        const auto it = impl_->shader_program_registry.find(handle.id);
        if (it == impl_->shader_program_registry.end() ||
            it.value().generation != handle.generation ||
            !it.value().pso ||
            !it.value().srb) {
            impl_->active_shader_program = {};
            return;
        }

        impl_->active_shader_program = handle;
        impl_->immediate_context->SetPipelineState(it.value().pso);
    }

    void DiligentRenderBackend::BindShaderTexture(std::string_view name, FrameBufferColorView view) {
        if (!view.IsValid() || !impl_->active_shader_program.IsValid()) {
            return;
        }

        auto program_it = impl_->shader_program_registry.find(impl_->active_shader_program.id);
        if (program_it == impl_->shader_program_registry.end() ||
            program_it.value().generation != impl_->active_shader_program.generation) {
            return;
        }
        DiligentShaderProgramData &program = program_it.value();

        for (DiligentTextureBinding &texture: program.textures) {
            if (std::string_view{texture.name} != name) {
                continue;
            }

            texture.texture_variable->Set(reinterpret_cast<Diligent::ITextureView *>(view.native_handle));
            if (texture.sampler_variable != nullptr) {
                texture.sampler_variable->Set(program.sampler);
            }
            return;
        }
    }

    void DiligentRenderBackend::BindShaderTexture(std::string_view name, FrameBufferDepthView view) {
        if (!view.IsValid() || !impl_->active_shader_program.IsValid()) {
            return;
        }

        auto program_it = impl_->shader_program_registry.find(impl_->active_shader_program.id);
        if (program_it == impl_->shader_program_registry.end() ||
            program_it.value().generation != impl_->active_shader_program.generation) {
            return;
        }
        DiligentShaderProgramData &program = program_it.value();

        for (DiligentTextureBinding &texture: program.textures) {
            if (std::string_view{texture.name} != name) {
                continue;
            }

            texture.texture_variable->Set(reinterpret_cast<Diligent::ITextureView *>(view.native_handle));
            if (texture.sampler_variable != nullptr) {
                texture.sampler_variable->Set(program.sampler);
            }
            return;
        }
    }

    void DiligentRenderBackend::BindShaderUniform(std::string_view name, std::span<const std::uint8_t> data) {
        if (data.empty() || !impl_->immediate_context || !impl_->active_shader_program.IsValid()) {
            return;
        }

        auto program_it = impl_->shader_program_registry.find(impl_->active_shader_program.id);
        if (program_it == impl_->shader_program_registry.end() ||
            program_it.value().generation != impl_->active_shader_program.generation) {
            return;
        }
        DiligentShaderProgramData &program = program_it.value();

        for (DiligentUniformBinding &uniform: program.uniforms) {
            if (std::string_view{uniform.name} != name || data.size() > uniform.byte_size) {
                continue;
            }

            UpdateBuffer(
                impl_->immediate_context,
                uniform.buffer,
                data.data(),
                static_cast<uint32_t>(data.size()));
            uniform.variable->Set(uniform.buffer);
            return;
        }
    }

    void DiligentRenderBackend::SetPerFrameProps(PerFrameProps props) {
        if (!impl_->per_frame_cb) {
            return;
        }


        PerFrameCB cb{.view_proj = props.camera.projection * props.camera.view, .frame_clock = props.frame_clock};
        UpdateBuffer(impl_->immediate_context,
                     impl_->per_frame_cb,
                     &cb, sizeof(cb));
    }

    void DiligentRenderBackend::SubmitBatch(const RenderBatch &batch) {
        const auto mat_it = impl_->material_registry.find(batch.material.id);
        const auto msh_it = impl_->mesh_registry.find(batch.mesh.id);

        if (mat_it == impl_->material_registry.end() ||
            msh_it == impl_->mesh_registry.end() ||
            mat_it.value().generation != batch.material.generation ||
            msh_it.value().generation != batch.mesh.generation) {
            return;
        }

        DiligentMaterialData &mat = mat_it.value();
        const DiligentMeshData &msh = msh_it.value();

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

        for (DiligentTextureBinding &texture: mat.textures) {
            if (!texture.texture.IsValid()) {
                continue;
            }

            const DiligentTextureSnapshot snapshot = GetTextureSnapshot(*impl_, texture.texture);
            if (!snapshot.texture_view || snapshot.revision == texture.bound_revision) {
                continue;
            }

            texture.texture_variable->Set(
                snapshot.texture_view,
                Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            texture.bound_revision = snapshot.revision;
        }

        impl_->immediate_context->CommitShaderResources(
            mat.srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const auto draw_instance = [this, &msh](const RenderInstance &inst) {
            PerObjectCB object_cb{inst.transform};
            UpdateBuffer(impl_->immediate_context,
                         impl_->per_object_cb,
                         &object_cb, sizeof(object_cb));

            Diligent::DrawIndexedAttribs draw;
            draw.IndexType = Diligent::VT_UINT32;
            draw.NumIndices = msh.index_count;
            draw.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
            impl_->immediate_context->DrawIndexed(draw);
        };

        for (const RenderInstance &inst: batch.InlineInstances()) {
            draw_instance(inst);
        }

        for (const RenderInstance &inst: batch.OverflowInstances()) {
            draw_instance(inst);
        }
    }

    void DiligentRenderBackend::Draw(std::uint32_t vertex_count, std::uint32_t instance_count) {
        if (vertex_count == 0u || instance_count == 0u || !impl_->immediate_context ||
            !impl_->active_shader_program.IsValid()) {
            return;
        }

        const auto program_it = impl_->shader_program_registry.find(impl_->active_shader_program.id);
        if (program_it == impl_->shader_program_registry.end() ||
            program_it.value().generation != impl_->active_shader_program.generation ||
            !program_it.value().pso ||
            !program_it.value().srb) {
            return;
        }

        impl_->immediate_context->SetPipelineState(program_it.value().pso);
        impl_->immediate_context->CommitShaderResources(
            program_it.value().srb,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::DrawAttribs draw;
        draw.NumVertices = vertex_count;
        draw.NumInstances = instance_count;
        draw.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
        impl_->immediate_context->Draw(draw);
    }

    std::string_view DiligentRenderBackend::LastError() const {
        return impl_ ? impl_->last_error : "backend unavailable";
    }
} // namespace CoreEngine
