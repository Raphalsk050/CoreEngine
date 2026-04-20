#include <memory>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

#include "core/i_game_app.h"
#include "core/application/application.h"
#include "core/ecs/world.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/render/material.h"
#include "core/render/primitive_type.h"
#include "core/render/camera.h"
#include "core/render/render_system.h"
#include "core/log/log.h"
#include "core/window/window_system.h"

class MyGameApp final : public CoreEngine::IGameApp {
public:
    MyGameApp() = default;

    void Init(const CoreEngine::EngineContext &context) override {
        const CoreEngine::MeshHandle cube_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Cube);
        const CoreEngine::MeshHandle plane_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Plane);
        const CoreEngine::MaterialHandle material =
                CoreEngine::Material::Unlit({.color = {1.2f, 0.6f, 1.0f, 1.0f}}).Resolve(context.render_system);

        cube_node_ = context.world.CreateNode("Cube");

        cube_node_.SetPosition(glm::vec3(2.0f, 0.0f, 0.0f));

        cube_node_.AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = cube_mesh,
                .material = material,
            });

        plane_node_ = context.world.CreateNode("Plane");
        plane_node_.AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = plane_mesh,
                .material = material,
            });

        plane_node_.SetPosition({0.f, -0.75f, 0.f});
        plane_node_.SetScale({3.f, 1.f, 3.f});
        plane_node_.SetRotation(glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 0.f, 1.f)));

        context.render_system.SetCamera(
            CoreEngine::Camera()
            .LookAt({0.f, 1.5f, -4.f}, {0.f, 0.f, 0.f})
            .Perspective(60.f, 1280, 720, 0.0001f, 10000.f));
    }

    void Update(const CoreEngine::FrameContext &frame) override {
        angle_ += frame.delta_time * 60.f;

        cube_node_.SetRotation(glm::angleAxis(glm::radians(angle_), glm::vec3(0.0f, 1.f, 0.f)));
        // plane_node_.SetRotation(
        //     glm::angleAxis(glm::radians(angle_), glm::vec3(0.0f, 1.f, 0.f)) + glm::angleAxis(
        //         glm::radians(180.0f), glm::vec3(0.0f, 0.f, 1.f)));
    }

    void Shutdown(const CoreEngine::EngineContext &) override {
    }

private:
    CoreEngine::Node cube_node_;
    CoreEngine::Node plane_node_;
    float angle_ = 0.f;
};

int main() {
    std::unique_ptr<CoreEngine::IGameApp> app = std::make_unique<MyGameApp>();

    CoreEngine::EngineConfig config;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.fullscreen = false;
    config.decorated = true;
    config.resizable = true;
    config.windowTitle = "CoreEngine - Mesh Demo";
    config.renderBackend = CoreEngine::RenderBackendType::DiligentD3D12;
    config.vsync = false;

    return CoreEngine::RunEngine(std::move(app), config);
}
