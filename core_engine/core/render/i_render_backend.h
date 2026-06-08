#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "core/render/camera_data.h"
#include "core/render/frame_buffer.h"
#include "core/render/material_desc.h"
#include "core/render/mesh_desc.h"
#include "core/render/render_batch.h"
#include "core/render/render_desc.h"
#include "core/render/texture_desc.h"
#include "core/window/native_window_handle.h"
#include "debug/depth_visualization.h"

namespace CoreEngine {
    inline constexpr std::size_t kMaxPbrCascades = 4u;
    inline constexpr std::size_t kMaxPbrPointLights = 8u;
    inline constexpr std::size_t kMaxPbrShadowedPointLights = 6u;
    inline constexpr std::size_t kPbrPointShadowFaceCount = 6u;

    struct DirectionalLightFrameData {
        Math::Vec3 direction{0.f, -1.f, 0.f};
        float illuminance_lux = 0.f;
        Math::Vec3 color{1.f, 1.f, 1.f};
        bool enabled = false;
    };

    struct EnvironmentLightFrameData {
        Math::Vec3 diffuse_irradiance{0.f, 0.f, 0.f};
        float intensity = 1.f;
        Math::Vec3 specular_radiance{0.f, 0.f, 0.f};
        float specular_intensity = 1.f;
        bool enabled = false;
        bool texture_ibl_enabled = false;
        float ibl_prefiltered_mip_count = 1.f;
    };

    struct PointLightFrameData {
        Math::Vec3 position{0.f, 0.f, 0.f};
        float range = 0.f;
        Math::Vec3 color{1.f, 1.f, 1.f};
        float luminous_intensity_cd = 0.f;
        float shadow_near_z = 0.05f;
        float shadow_bias = 0.005f;
        float shadow_normal_bias = 0.03f;
        bool casts_shadows = false;
        std::uint32_t shadow_index = 0u;
    };

    struct PbrShadowFrameData {
        std::array<Math::Mat4, kMaxPbrCascades> directional_shadow_view_proj{};
        Math::Vec4 directional_shadow_splits{};
        Math::Vec4 directional_shadow_params{};
        Math::Vec4 directional_shadow_extra{};
        Math::Vec4 point_shadow_params{};
        std::array<Math::Mat4, kMaxPbrShadowedPointLights * kPbrPointShadowFaceCount> point_shadow_view_proj{};
    };

    struct PerFrameProps {
        const CameraData &camera;
        Math::Vec4 frame_clock;
        Math::Vec3 camera_position{0.f, 0.f, 0.f};
        float exposure = 1.f;
        DirectionalLightFrameData directional_light{};
        EnvironmentLightFrameData environment_light{};
        std::array<PointLightFrameData, kMaxPbrPointLights> point_lights{};
        std::uint32_t point_light_count = 0u;
        PbrShadowFrameData shadows{};
        Math::Vec4 reflection_probe_position_radius{};
        Math::Vec4 reflection_probe_params{};
        Math::Vec4 pbr_debug_params{};
    };

    struct PbrGlobalResources {
        TextureViewHandle directional_shadow_map{};
        TextureViewHandle point_shadow_map{};
        TextureViewHandle irradiance_map{};
        TextureViewHandle prefiltered_specular_map{};
        TextureViewHandle brdf_lut{};
    };

    class IRenderBackend {
    public:
        virtual ~IRenderBackend() = default;

        [[nodiscard]] virtual bool Initialize(const RenderDesc &desc, NativeWindowHandle native_window) = 0;

        virtual void BeginFrame() = 0;

        virtual void Clear(const RenderClearColor &clear_color) = 0;

        virtual void BeginImGuiFrame() = 0;

        virtual void RenderImGui() = 0;

        virtual void EndFrame() = 0;

        virtual void Resize(int width, int height) = 0;

        virtual void Shutdown() = 0;

        [[nodiscard]] virtual FrameBufferHandle CreateFrameBuffer(const FrameBufferDesc &desc) = 0;

        virtual void DestroyFrameBuffer(FrameBufferHandle handle) = 0;

        [[nodiscard]] virtual TextureHandle CreateTexture(const TextureDesc &desc) = 0;

        [[nodiscard]] virtual TextureViewHandle CreateTextureView(const TextureViewDesc &desc) = 0;

        virtual void DestroyTextureView(TextureViewHandle handle) = 0;

        virtual void SetFrameBuffer(FrameBufferHandle handle) = 0;

        virtual void SetSwapChainFrameBuffer() = 0;

        virtual void SetRenderTargets(TextureViewHandle color_view, TextureViewHandle depth_view) = 0;

        [[nodiscard]] virtual FrameBufferColorView GetFrameBufferColorView(FrameBufferHandle handle) const = 0;

        [[nodiscard]] virtual FrameBufferDepthView GetFrameBufferDepthView(FrameBufferHandle handle) const = 0;

        virtual void RenderDepthToColor(FrameBufferDepthView source, FrameBufferHandle destination,
                                        const DepthVisualizationDesc &desc) = 0;

        virtual void CompositeFrameBuffer(FrameBufferHandle source, const PostProcessDesc &post_process) = 0;

        [[nodiscard]] virtual MeshHandle UploadMesh(const MeshDesc &desc) = 0;

        virtual void DestroyMesh(MeshHandle handle) = 0;

        [[nodiscard]] virtual MaterialHandle ResolveMaterial(const MaterialDesc &desc) = 0;

        [[nodiscard]] virtual ShaderProgramHandle CreateShaderProgram(const ShaderProgramDesc &desc) = 0;

        [[nodiscard]] virtual TextureHandle LoadTexture2D(const TextureLoadDesc &desc) = 0;

        [[nodiscard]] virtual TextureHandle LoadTexture2DAsync(const TextureLoadDesc &desc) = 0;

        [[nodiscard]] virtual TextureLoadState GetTextureLoadState(TextureHandle handle) const = 0;

        [[nodiscard]] virtual bool SaveTextureAsDds(TextureHandle handle, std::string_view path) = 0;

        virtual void DestroyTexture(TextureHandle handle) = 0;

        virtual void DestroyShaderProgram(ShaderProgramHandle handle) = 0;

        virtual void UseShaderProgram(ShaderProgramHandle handle) = 0;

        virtual void BindShaderTexture(std::string_view name, TextureHandle handle) = 0;

        virtual void BindShaderTexture(std::string_view name, FrameBufferColorView view) = 0;

        virtual void BindShaderTexture(std::string_view name, FrameBufferDepthView view) = 0;

        virtual void BindShaderTexture(std::string_view name, TextureViewHandle view) = 0;

        virtual void BindShaderUniform(std::string_view name, std::span<const std::uint8_t> data) = 0;

        virtual void SetPerFrameProps(PerFrameProps props) = 0;

        virtual void SetPbrGlobalResources(const PbrGlobalResources &resources) = 0;

        virtual void SubmitBatch(const RenderBatch &batch) = 0;

        virtual void SubmitGeometryBatch(const GeometryBatch &batch) = 0;

        virtual void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) = 0;

        [[nodiscard]] virtual std::string_view LastError() const = 0;
    };
} // namespace CoreEngine
