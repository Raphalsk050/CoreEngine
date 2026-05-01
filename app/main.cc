#include <memory>

#include "imgui.h"

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
#include "shaders/test_shader.h"

struct TestShaderProps {
    alignas(16) CoreEngine::Math::Vec4 color{1.f, 1.f, 1.f, 1.f};
    float alpha = 1.0f;
};

// this example here serves just to demonstrates that the Render pass is working
// to try this just change the .stage from BeforeMainScene to BeforeImGui to see
// that all the main scene will change to a green color and the imgui panel will
// continue to be shown :)
class ClearScreenPass final : public CoreEngine::IRenderPass {
public:
    [[nodiscard]] CoreEngine::RenderPassDesc Describe() const override {
        return {
            .name = "ClearScreenPass",
            .stage = CoreEngine::RenderPassStage::FrameSetup,
            .order = 0,
        };
    }

    void Execute(CoreEngine::RenderPassContext &context) override {
        context.SetSwapChainFrameBuffer();

        context.Clear({
            .r = 0.05f,
            .g = 0.35f,
            .b = 0.15f,
            .a = 1.0f,
        });
    }
};

class MyGameApp final : public CoreEngine::IGameApp {
public:
    MyGameApp() = default;

    void Init(const CoreEngine::EngineContext &context) override {
        if (!player_controller_.Init(context)) {
            CoreEngine::Log::Warn("Game", "Failed to bind one or more player input actions");
        }

        clear_pass_ = context.render_system.AddRenderPass(
            std::make_unique<ClearScreenPass>());

        const CoreEngine::MeshHandle cube_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Cube);
        const CoreEngine::MeshHandle plane_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Plane);
        const CoreEngine::MaterialHandle cube_material =
                CoreEngine::Material::Unlit({.color = {1.2f, 0.6f, 1.0f, 1.0f}}).Resolve(context.render_system);

        TestShaderProps shader_props;
        shader_props.color = {1.0, 0.0, 0.0, 1.0};
        shader_props.alpha = 0.1f;

        auto custom_material = CoreEngine::Material::Custom(Game::Shaders::kTestVS, Game::Shaders::kTestPS,
                                                            shader_props);

        const CoreEngine::MaterialHandle player_material = custom_material.Resolve(context.render_system);

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
                .material = player_material,
            });


        CoreEngine::CameraComponent camera{.priority = 1, .enabled = false};
        camera_node_ = context.world.CreateNode("MainCamera");
        secondary_camera_node_ = context.world.CreateNode("SecondaryCamera");
        secondary_camera_node_.AddComponent<CoreEngine::CameraComponent>(camera);
        secondary_camera_node_.AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = cube_mesh,
                .material = player_material,
            });

        camera_node_.AddComponent<CoreEngine::CameraComponent>();
        camera_node_.SetPosition({0.0f, 1.5f, -4.0f});
        camera_node_.AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = cube_mesh,
                .material = player_material,
            });

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

        ApplyCursorMode(context.window_system, CoreEngine::WindowCursorMode::CURSOR_NORMAL);
    }

    void Update(const CoreEngine::FrameContext &frame) override {
        if (frame.input_system.WasKeyPressed(CoreEngine::Key::Escape)) {
            CoreEngine::Application::RequestShutdown();
        }

        player_controller_.Update(frame);
        RenderDebugUi(frame);

        auto &editor_camera = secondary_camera_node_.GetComponent<CoreEngine::CameraComponent>();

        if (frame.input_system.WasKeyPressed(CoreEngine::Key::Tab)) {
            switch (current_cursor_mode_) {
                case CoreEngine::WindowCursorMode::CURSOR_NORMAL:
                    ApplyCursorMode(frame.window_system, CoreEngine::WindowCursorMode::CURSOR_CONSTRAINED_AND_HIDDEN);
                    editor_camera.enabled = false;
                    editor_camera.priority = 0;
                    break;
                case CoreEngine::WindowCursorMode::CURSOR_CONSTRAINED_AND_HIDDEN:
                default:
                    ApplyCursorMode(frame.window_system, CoreEngine::WindowCursorMode::CURSOR_NORMAL);
                    editor_camera.enabled = true;
                    editor_camera.priority = 10;
                    break;
            }
        }
    }

    void Shutdown(const CoreEngine::EngineContext &context) override {
        context.render_system.RemoveRenderPass(clear_pass_);
        clear_pass_ = {};
        player_controller_.DetachCameraController();
        player_controller_.Unpossess();
    }

private:
    void RenderDebugUi(const CoreEngine::FrameContext &frame) {
        if (ImGui::GetCurrentContext() == nullptr) {
            return;
        }

        ImGui::SetNextWindowPos(ImVec2{16.0f, 16.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2{360.0f, 0.0f}, ImGuiCond_FirstUseEver);

        if (ImGui::Begin("CoreEngine Debug")) {
            ImGui::Text("FPS: %.0f", 1.0 / frame.delta_time);
        }
        ImGui::End();
    }

    void ApplyCursorMode(CoreEngine::WindowSystem &window_system, CoreEngine::WindowCursorMode cursor_mode) {
        if (window_system.SetWindowCursorMode(cursor_mode)) {
            current_cursor_mode_ = cursor_mode;
        }
    }

    CoreEngine::Node plane_node_;
    CoreEngine::Node camera_node_;
    CoreEngine::Node secondary_camera_node_;
    Game::ThirdPersonCameraController third_person_camera_controller_;
    Game::PlayerPawn player_pawn_;
    Game::PlayerController player_controller_;
    CoreEngine::WindowCursorMode current_cursor_mode_ = CoreEngine::WindowCursorMode::CURSOR_NORMAL;
    float debug_value_ = 0.5f;
    float debug_color_[3] = {0.25f, 0.55f, 0.9f};
    int debug_counter_ = 0;
    CoreEngine::RenderPassHandle clear_pass_{};
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
    config.enableImGui = true;

    return CoreEngine::RunEngine(std::move(app), config);
}
