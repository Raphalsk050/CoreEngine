#include "core/render/render_system.h"

#include <cstddef>
#include <utility>

#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/time/frame_clock.h"
#include "core/ecs/components/camera_component.h"
#include "core/log/logger.h"
#include "core/render/primitives.h"

namespace CoreEngine {
    RenderSystem::RenderSystem(std::unique_ptr<IRenderBackend> backend)
        : backend_(std::move(backend)) {
    }

    bool RenderSystem::Initialize(const RenderDesc &desc, NativeWindowHandle native_window) {
        desc_ = desc;
        surface_width_ = desc.width > 0 ? desc.width : 1;
        surface_height_ = desc.height > 0 ? desc.height : 1;

        default_camera_ = Camera{}
                .LookAt({0.f, 0.f, -5.f}, {0.f, 0.f, 0.f})
                .Perspective(60.f,
                             static_cast<float>(surface_width_),
                             static_cast<float>(surface_height_),
                             0.01f,
                             1000.f)
                .GetCameraData();

        initialized_ = backend_ != nullptr && backend_->Initialize(desc, native_window);
        return initialized_;
    }

    void RenderSystem::BeginImGuiFrame() const {
        if (!initialized_ || backend_ == nullptr || !desc_.enable_imgui) {
            return;
        }

        backend_->BeginImGuiFrame();
    }

    void RenderSystem::RenderFrame(World &world, FrameClock frame_clock) {
        if (!initialized_ || backend_ == nullptr) {
            return;
        }

        auto view = world.View<TransformComponent, MeshRendererComponent>();
        accumulator_.Clear();
        accumulator_.Reserve(static_cast<std::size_t>(view.size_hint()));

        for (const auto &[entity, transform, renderer]: view.each()) {
            (void) entity;

            if (!renderer.visible || !renderer.material.IsValid() || !renderer.mesh.IsValid()) {
                continue;
            }

            accumulator_.Add(renderer.material, renderer.mesh, transform.WorldMatrix());
        }

        const CameraData active_camera = has_manual_camera_override_
                                             ? manual_camera_override_
                                             : ResolveWorldCamera(world);

        PerFrameProps pros{
            .camera = active_camera,
            .frame_clock = Math::Vec4(frame_clock.TickSeconds(), static_cast<float>(frame_clock.TotalSeconds()), 0.0f,
                                      0.0f)
        };
        backend_->SetPerFrameProps(pros);
        backend_->BeginFrame();
        backend_->Clear(desc_.clear_color);

        for (const RenderBatch &batch: accumulator_.Batches()) {
            backend_->SubmitBatch(batch);
        }

        if (desc_.enable_imgui) {
            backend_->RenderImGui();
        }

        backend_->EndFrame();
    }

    MeshHandle RenderSystem::GetOrCreatePrimitive(PrimitiveType type) {
        const auto index = static_cast<std::size_t>(type);
        if (index >= primitive_cache_.size()) {
            return {};
        }

        MeshHandle &cached = primitive_cache_[index];
        if (cached.IsValid()) {
            return cached;
        }

        const MeshDesc desc = Primitives::MeshFor(type);
        if (!desc.IsValid()) {
            return {};
        }

        cached = CreateMesh(desc);
        return cached;
    }

    MeshHandle RenderSystem::CreateMesh(const MeshDesc &desc) {
        if (!initialized_ || backend_ == nullptr || !desc.IsValid()) {
            return {};
        }

        return backend_->UploadMesh(desc);
    }

    MaterialHandle RenderSystem::ResolveMaterial(const MaterialDesc &desc) {
        if (!initialized_ || backend_ == nullptr) {
            return {};
        }

        return backend_->ResolveMaterial(desc);
    }

    void RenderSystem::DestroyMesh(MeshHandle handle) {
        if (!handle.IsValid() || backend_ == nullptr) {
            return;
        }

        backend_->DestroyMesh(handle);

        for (MeshHandle &primitive: primitive_cache_) {
            if (primitive == handle) {
                primitive = {};
            }
        }
    }

    void RenderSystem::SetCamera(const Camera &camera) {
        manual_camera_override_ = camera.GetCameraData();
        has_manual_camera_override_ = true;
    }

    void RenderSystem::SetCamera(const CameraData &camera_data) {
        manual_camera_override_ = camera_data;
        has_manual_camera_override_ = true;
    }

    void RenderSystem::ClearCameraOverride() {
        has_manual_camera_override_ = false;
    }

    void RenderSystem::Resize(int width, int height) {
        if (!initialized_ || backend_ == nullptr) {
            return;
        }
        surface_width_ = width > 0 ? width : 1;
        surface_height_ = height > 0 ? height : 1;
        backend_->Resize(surface_width_, surface_height_);
    }

    void RenderSystem::Shutdown() {
        if (backend_ != nullptr) {
            backend_->Shutdown();
        }

        primitive_cache_.fill({});
        initialized_ = false;
    }

    bool RenderSystem::IsInitialized() const {
        return initialized_;
    }

    std::string_view RenderSystem::LastError() const {
        return backend_ != nullptr ? backend_->LastError() : "Render backend is not available";
    }

    IRenderContext &RenderSystem::Context() {
        return *this;
    }

    CameraData RenderSystem::ResolveWorldCamera(World &world) const {
        const TransformComponent *best_transform = nullptr;
        const CameraComponent *best_camera = nullptr;

        // TODO(rafael): searches through all entities for now that have the best camera, find a better solution in the future
        auto view = world.View<TransformComponent, CameraComponent>();
        for (auto [entity, transform, camera]: view.each()) {
            (void) entity;

            if (!camera.enabled) {
                continue;
            }

            if (best_camera == nullptr || camera.priority > best_camera->priority) {
                best_camera = &camera;
                best_transform = &transform;
            }
        }

        if (best_transform == nullptr || best_camera == nullptr) {
            return default_camera_;
        }

        return BuildCameraData(*best_transform, *best_camera);
    }

    CameraData RenderSystem::BuildCameraData(const TransformComponent &transform, const CameraComponent &camera) const {
        const int width = surface_width_ > 0 ? surface_width_ : 1;
        const int height = surface_height_ > 0 ? surface_height_ : 1;

        const float aspect_ratio = camera.aspect_mode == CameraAspectMode::Fixed
                                       ? camera.fixed_aspect_ratio
                                       : static_cast<float>(width) / static_cast<float>(height);

        const Math::Vec3 forward = transform.rotation * Math::Vec3{0.f, 0.f, 1.f};
        const Math::Vec3 up = transform.rotation * Math::Vec3{0.f, 1.f, 0.f};

        CameraData data;
        data.view = Math::LookAtLH(transform.position, transform.position + forward, up);

        if (camera.projection_type == CameraProjectionType::Perspective) {
            data.projection = Math::PerspectiveLH(Math::Deg2Rad(camera.fov_y_degrees), aspect_ratio, camera.near_z,
                                                  camera.far_z);
            return data;
        }

        const float half_height = camera.orthographic_height * 0.5f;
        const float half_width = half_height * aspect_ratio;

        data.projection = Math::OrthoLH(
            -half_width,
            half_width,
            -half_height,
            half_height,
            camera.near_z,
            camera.far_z);

        return data;
    }
} // namespace CoreEngine
