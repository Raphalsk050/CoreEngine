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
#include "core/render/material.h"
#include "core/render/primitive_type.h"
#include "core/render/render_system.h"
#include "core/window/window_system.h"

struct TestShaderProps {
    alignas(16) CoreEngine::Math::Vec4 color{1.f, 1.f, 1.f, 1.f};
    float alpha = 1.0f;
};

class MyGameApp final : public CoreEngine::IGameApp {
public:
    MyGameApp() = default;

    void Init(const CoreEngine::EngineContext &context) override {
        if (!player_controller_.Init(context)) {
            CoreEngine::Log::Warn("Game", "Failed to bind one or more player input actions");
        }

        auto model = context.render_system.LoadModelAsyncFuture(CoreEngine::ModelLoadDesc{
            .path = "app/assets/models/animal-crab.obj",
            .merge_submeshes = true
        });

        model.Then([=, this](const CoreEngine::FutureResult<CoreEngine::ModelHandle> &result) {
            auto mesh_handle = context.render_system.GetModelMesh(result.Value(), 0);


            TestShaderProps shader_props;
            shader_props.color = {1.0, 0.0, 0.0, 1.0};
            shader_props.alpha = 0.1f;


            auto custom_material = CoreEngine::MaterialBuilder{}
                    .Vertex("app/assets/shaders/custom_shader_vertex.hlsl", true)
                    .Pixel("app/assets/shaders/custom_shader_pixel.hlsl", true)
                    .Texture("g_Albedo", player_texture_)
                    .Properties(shader_props)
                    .Build();

            const CoreEngine::MaterialHandle player_material = custom_material.Resolve(context.render_system);


            CoreEngine::Node map_node = context.world.CreateNode("MapNode");
            map_node.AddComponent<CoreEngine::MeshRendererComponent>(CoreEngine::MeshRendererComponent{
                .mesh = mesh_handle,
                .material = player_material,
            });

            // map_node.SetRotation(CoreEngine::Math::AngleAxis(90.0f, {1.0f, 0.0f, 0.0f}));
            // map_node.SetScale({0.01f, 0.01f, 0.01f});
        });

        const CoreEngine::MeshHandle cube_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Cube);
        const CoreEngine::MeshHandle plane_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Plane);

        player_texture_ = context.render_system.LoadTexture2DAsync(CoreEngine::TextureLoadDesc{
            .path = "app/assets/textures/uv_mapping.png",
            .format = CoreEngine::TextureFormat::RGBA8Unorm,
            .generate_mipmaps = true
        });

        floor_texture_ = context.render_system.LoadTexture2DAsync(CoreEngine::TextureLoadDesc{
            .path = "app/assets/textures/uv_mapping.png",
            .format = CoreEngine::TextureFormat::RGBA8Unorm,
            .generate_mipmaps = true
        });

        TestShaderProps shader_props;
        shader_props.color = {1.0, 0.0, 0.0, 1.0};
        shader_props.alpha = 0.1f;

        auto custom_material = CoreEngine::MaterialBuilder{}
                .Vertex("app/assets/shaders/custom_shader_vertex.hlsl", true)
                .Pixel("app/assets/shaders/custom_shader_pixel.hlsl", true)
                .Texture("g_Albedo", player_texture_)
                .Properties(shader_props)
                .Build();

        auto floor_material = CoreEngine::MaterialBuilder{}
                .Vertex("app/assets/shaders/custom_shader_vertex.hlsl", true)
                .Pixel("app/assets/shaders/custom_shader_pixel.hlsl", true)
                .Texture("g_Albedo", floor_texture_)
                .Properties(shader_props)
                .Build();

        const CoreEngine::MaterialHandle player_material = custom_material.Resolve(context.render_system);

        const CoreEngine::MaterialHandle plane_material = floor_material.Resolve(context.render_system);

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
                .material = plane_material,
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

        UpdateFpsCounter(frame);
    }

    void ApplyCursorMode(CoreEngine::WindowSystem &window_system, CoreEngine::WindowCursorMode cursor_mode) {
        if (window_system.SetWindowCursorMode(cursor_mode)) {
            current_cursor_mode_ = cursor_mode;
        }
    }

    void UpdateFpsCounter(const CoreEngine::FrameContext &frame) {
        if (update_counter_ > update_frame_delay_) {
            current_delta_time_ = frame.delta_time;
            int current_fps = 1.0 / current_delta_time_;

            if (current_fps > max_fps_) {
                max_fps_ = current_fps;
            }

            if (current_fps < min_fps_) {
                min_fps_ = current_fps;
            }

            update_counter_ = 0;
        }

        if (ImGui::Begin("CoreEngine Debug")) {
            ImGui::Text("FPS: %.0f", 1.0 / current_delta_time_);
            ImGui::Text("FPS MAX: %d", max_fps_);
            ImGui::Text("FPS MIN: %d", min_fps_);
            if (ImGui::Button("Reset statistics")) {
                max_fps_ = 0;
                min_fps_ = INT32_MAX;
            }
        }
        ImGui::End();

        last_delta_time_ = current_delta_time_;
        ++update_counter_;
    }

    CoreEngine::Node plane_node_;
    CoreEngine::Node camera_node_;
    CoreEngine::Node secondary_camera_node_;
    CoreEngine::TextureHandle player_texture_;
    CoreEngine::TextureHandle floor_texture_;
    Game::ThirdPersonCameraController third_person_camera_controller_;
    Game::PlayerPawn player_pawn_;
    Game::PlayerController player_controller_;
    CoreEngine::WindowCursorMode current_cursor_mode_ = CoreEngine::WindowCursorMode::CURSOR_NORMAL;
    float debug_value_ = 0.5f;
    float debug_color_[3] = {0.25f, 0.55f, 0.9f};
    int debug_counter_ = 0;
    float last_delta_time_ = 0.0f;
    float current_delta_time_ = 0.0f;
    int update_counter_ = 0;
    int update_frame_delay_ = 500;
    int max_fps_ = 0;
    int min_fps_ = INT32_MAX;
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
    config.renderBackend = CoreEngine::RenderBackendType::DiligentD3D11;
    config.vsync = false;
    config.enableImGui = true;

    return CoreEngine::RunEngine(std::move(app), config);
}
