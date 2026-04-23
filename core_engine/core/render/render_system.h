#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>

#include "core/render/camera.h"
#include "core/render/camera_data.h"
#include "core/render/i_render_backend.h"
#include "core/render/i_render_context.h"
#include "core/render/mesh_desc.h"
#include "core/render/primitive_type.h"
#include "core/render/render_batch.h"

namespace CoreEngine {
    class World;
    struct CameraComponent;
    struct TransformComponent;

    class RenderSystem final : public IRenderContext {
    public:
        explicit RenderSystem(std::unique_ptr<IRenderBackend> backend);

        [[nodiscard]] bool Initialize(const RenderDesc &desc, NativeWindowHandle native_window);

        void RenderFrame(World &world);

        [[nodiscard]] MeshHandle GetOrCreatePrimitive(PrimitiveType type) override;

        [[nodiscard]] MeshHandle CreateMesh(const MeshDesc &desc) override;

        [[nodiscard]] MaterialHandle ResolveMaterial(const MaterialDesc &desc) override;

        void DestroyMesh(MeshHandle handle);

        void SetCamera(const Camera &camera);

        void SetCamera(const CameraData &camera_data);

        void ClearCameraOverride();

        void Resize(int width, int height);

        void Shutdown();

        [[nodiscard]] bool IsInitialized() const;

        [[nodiscard]] std::string_view LastError() const;

        [[nodiscard]] IRenderContext &Context();

    private:
        [[nodiscard]] CameraData ResolveWorldCamera(World &world) const;

        [[nodiscard]] CameraData BuildCameraData(const TransformComponent &transform,
                                                 const CameraComponent &camera) const;

        static constexpr std::size_t kPrimitiveCount = static_cast<std::size_t>(PrimitiveType::Count);

        std::unique_ptr<IRenderBackend> backend_;
        RenderDesc desc_{};
        CameraData manual_camera_override_{};
        CameraData default_camera_{};
        bool has_manual_camera_override_ = false;

        int surface_width_ = 1;
        int surface_height_ = 1;

        BatchAccumulator accumulator_;
        std::array<MeshHandle, kPrimitiveCount> primitive_cache_{};
        bool initialized_ = false;
    };
} // namespace CoreEngine
