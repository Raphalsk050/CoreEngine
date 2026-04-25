#include <memory>

#include "core/math/math.h"

#include "player_controller.h"
#include "player_pawn.h"
#include "third_person_camera_controller.h"
#include "core/i_game_app.h"
#include "core/application/application.h"
#include "core/ecs/world.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/camera_component.h"
#include "core/input/input_codes.h"
#include "core/input/input_system.h"
#include "core/log/log.h"
#include "core/math/math.h"
#include "core/render/camera.h"
#include "core/render/material.h"
#include "core/render/primitive_type.h"
#include "core/render/render_system.h"
#include "core/window/window_system.h"

class MyGameApp final : public CoreEngine::IGameApp {
public:
    MyGameApp() = default;

    void Init(const CoreEngine::EngineContext &context) override {
        if (!player_controller_.Init(context)) {
            CoreEngine::Log::Warn("Game", "Failed to bind one or more player input actions");
        }

        const CoreEngine::MeshHandle cube_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Cube);
        const CoreEngine::MeshHandle plane_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Plane);
        const CoreEngine::MaterialHandle cube_material =
                CoreEngine::Material::Unlit({.color = {1.2f, 0.6f, 1.0f, 1.0f}}).Resolve(context.render_system);

        const CoreEngine::MaterialHandle plane_material =
                CoreEngine::Material::Unlit({.color = {0.5f, 0.6f, 1.0f, 1.0f}}).Resolve(context.render_system);

        player_pawn_ = Game::PlayerPawn(
            context.world.CreateNode("Player"),
            Game::MovementComponent{
                .crouch_speed = 1.5f,
                .walk_speed = 3.0f,
                .run_speed = 6.0f,
                .default_movement_type = Game::MovementType::Walk,
            });

        player_pawn_.Node().SetPosition(CoreEngine::Math::Vec3(2.0f, 0.0f, 0.0f));
        player_pawn_.Node().AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = cube_mesh,
                .material = cube_material,
            });

        camera_node_ = context.world.CreateNode("MainCamera");
        camera_node_.AddComponent<CoreEngine::CameraComponent>();

        camera_node_.SetPosition({0.0f, 1.5f, -4.0f});

        third_person_camera_controller_.Attach(camera_node_, player_pawn_.Node());
        third_person_camera_controller_.SetFocusOffset({0.0f, 1.25f, 0.0f});
        third_person_camera_controller_.SetDistance(4.0f);

        player_controller_.AttachCameraController(third_person_camera_controller_);
        player_controller_.Possess(player_pawn_);

        plane_node_ = context.world.CreateNode("Plane");
        plane_node_.AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = plane_mesh,
                .material = plane_material,
            });

        plane_node_.SetPosition({0.f, -0.75f, 0.f});
        plane_node_.SetScale({3.f, 1.f, 3.f});
        plane_node_.SetRotation(
            CoreEngine::Math::AngleAxis(CoreEngine::Math::Deg2Rad(180.0f), CoreEngine::Math::Vec3(0.0f, 0.f, 1.f)));

        context.window_system.SetWindowCursorMode(CoreEngine::WindowCursorMode::CURSOR_CONSTRAINED_AND_HIDDEN);
    }

    void Update(const CoreEngine::FrameContext &frame) override {
        if (frame.input_system.WasKeyPressed(CoreEngine::Key::Escape)) {
            CoreEngine::Application::RequestShutdown();
        }

        player_controller_.Update(frame);

        if (frame.input_system.WasKeyPressed(CoreEngine::Key::Tab)) {
            switch (current_cursor_mode_) {
                case CoreEngine::WindowCursorMode::CURSOR_NORMAL:
                    frame.window_system.
                            SetWindowCursorMode(CoreEngine::WindowCursorMode::CURSOR_CONSTRAINED_AND_HIDDEN);
                    current_cursor_mode_ = CoreEngine::WindowCursorMode::CURSOR_CONSTRAINED_AND_HIDDEN;
                    break;
                case CoreEngine::WindowCursorMode::CURSOR_CONSTRAINED_AND_HIDDEN:
                    frame.window_system.SetWindowCursorMode(CoreEngine::WindowCursorMode::CURSOR_NORMAL);
                    current_cursor_mode_ = CoreEngine::WindowCursorMode::CURSOR_NORMAL;
                    break;
                default:
                    frame.window_system.SetWindowCursorMode(CoreEngine::WindowCursorMode::CURSOR_NORMAL);
                    current_cursor_mode_ = CoreEngine::WindowCursorMode::CURSOR_NORMAL;
                    break;
            }
        }
    }

    void Shutdown(const CoreEngine::EngineContext &) override {
        player_controller_.DetachCameraController();
        player_controller_.Unpossess();
    }

private:
    CoreEngine::Node plane_node_;
    CoreEngine::Node camera_node_;
    Game::ThirdPersonCameraController third_person_camera_controller_;
    Game::PlayerPawn player_pawn_;
    Game::PlayerController player_controller_;
    CoreEngine::WindowCursorMode current_cursor_mode_ = CoreEngine::WindowCursorMode::CURSOR_CONSTRAINED_AND_HIDDEN;
};

int main() {
    std::unique_ptr<CoreEngine::IGameApp> app = std::make_unique<MyGameApp>();

    CoreEngine::EngineConfig config;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.fullscreen = false;
    config.decorated = true;
    config.resizable = true;
    config.windowTitle = "CoreEngine - Player Input Demo";
    config.renderBackend = CoreEngine::RenderBackendType::DiligentVulkan;
    config.vsync = false;

    return CoreEngine::RunEngine(std::move(app), config);
}
