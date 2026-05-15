#include <cstdint>
#include <memory>
#include "imgui.h"
#include "core/math/math.h"
#include "gameplay/systems/armor_system.h"
#include "gameplay/systems/bounty_beacon_system.h"
#include "gameplay/systems/capture_system.h"
#include "gameplay/systems/combat_system.h"
#include "gameplay/systems/crafting_system.h"
#include "gameplay/systems/economy_result_system.h"
#include "gameplay/systems/extraction_system.h"
#include "gameplay/systems/inventory_system.h"
#include "gameplay/systems/match_session_system.h"
#include "gameplay/systems/network_player_system.h"
#include "gameplay/systems/pve_ai_system.h"
#include "gameplay/systems/target_chain_system.h"
#include "player.h"
#include "core/i_game_app.h"
#include "core/application/application.h"
#include "core/ecs/world.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/camera_component.h"
#include "core/input/input_codes.h"
#include "core/input/input_system.h"
#include "core/online/steam/steam_multiplayer_debug_panel.h"
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
        world_ = &context.world;
        network_system_ = &context.network_system;
        network_replicator_ = &context.network_replicator;

        player_.Initialize(context);

        const CoreEngine::MeshHandle cube_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Cube);
        const CoreEngine::MeshHandle plane_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Plane);

        floor_texture_ = context.render_system.LoadTexture2DAsync(CoreEngine::TextureLoadDesc{
            .path = "app/assets/textures/uv_mapping.png",
            .data = {},
            .format = CoreEngine::TextureFormat::RGBA8Unorm,
            .generate_mipmaps = true
        });

        TestShaderProps shader_props;
        shader_props.color = {1.0f, 0.0f, 0.0f, 1.0f};
        shader_props.alpha = 0.1f;

        auto floor_material = CoreEngine::MaterialBuilder{}
                .Vertex("app/assets/shaders/custom_shader_vertex.hlsl", true)
                .Pixel("app/assets/shaders/custom_shader_pixel.hlsl", true)
                .Texture("g_Albedo", floor_texture_)
                .Properties(shader_props)
                .Build();

        const CoreEngine::MaterialHandle plane_material = floor_material.Resolve(context.render_system);

        CoreEngine::CameraComponent camera{.priority = 1, .enabled = false};
        secondary_camera_node_ = context.world.CreateNode("SecondaryCamera");
        secondary_camera_node_.AddComponent<CoreEngine::CameraComponent>(camera);
        secondary_camera_node_.AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = cube_mesh,
                .material = plane_material,
            });

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

        network_player_system_.Initialize(context.render_system);
        ApplyCursorMode(context.window_system, CoreEngine::WindowCursorMode::CURSOR_NORMAL);
    }

    void FixedUpdate(const CoreEngine::SimulationFrame &frame) override {
        player_.FixedUpdate(frame);

        if (world_ == nullptr || network_system_ == nullptr || network_replicator_ == nullptr) {
            return;
        }

        const Game::GameplaySystemContext context{
            .world = *world_,
            .network_system = *network_system_,
            .network_replicator = *network_replicator_,
            .frame = frame,
        };

        network_player_system_.FixedUpdate(context);
        match_session_system_.FixedUpdate(context);
        combat_system_.FixedUpdate(context);
        armor_system_.FixedUpdate(context);
        bounty_beacon_system_.FixedUpdate(context);
        capture_system_.FixedUpdate(context);
        inventory_system_.FixedUpdate(context);
        crafting_system_.FixedUpdate(context);
        extraction_system_.FixedUpdate(context);
        target_chain_system_.FixedUpdate(context);
        pve_ai_system_.FixedUpdate(context);
        economy_result_system_.FixedUpdate(context);
    }

    void Update(const CoreEngine::FrameContext &frame) override {
        if (frame.input_system.WasKeyPressed(CoreEngine::Key::Escape)) {
            CoreEngine::Application::RequestShutdown();
        }

        player_.Update(frame);
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
        (void) context;
        network_player_system_.Shutdown();
        player_.Shutdown();
        world_ = nullptr;
        network_system_ = nullptr;
        network_replicator_ = nullptr;
    }

private:
    void RenderDebugUi(const CoreEngine::FrameContext &frame) {
        if (ImGui::GetCurrentContext() == nullptr) {
            return;
        }

        ImGui::SetNextWindowPos(ImVec2{16.0f, 16.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2{360.0f, 0.0f}, ImGuiCond_FirstUseEver);

        UpdateFpsCounter(frame);
        steam_multiplayer_debug_panel_.Render(frame.online_system);
    }

    void ApplyCursorMode(CoreEngine::WindowSystem &window_system, CoreEngine::WindowCursorMode cursor_mode) {
        if (window_system.SetWindowCursorMode(cursor_mode)) {
            current_cursor_mode_ = cursor_mode;
        }
    }

    void UpdateFpsCounter(const CoreEngine::FrameContext &frame) {
        if (update_counter_ > update_frame_delay_) {
            current_delta_time_ = frame.delta_time;
            const int current_fps = static_cast<int>(1.0f / current_delta_time_);

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

    Game::Player player_;
    Game::NetworkPlayerSystem network_player_system_;
    Game::MatchSessionSystem match_session_system_;
    Game::TargetChainSystem target_chain_system_;
    Game::BountyBeaconSystem bounty_beacon_system_;
    Game::CaptureSystem capture_system_;
    Game::CombatSystem combat_system_;
    Game::ArmorSystem armor_system_;
    Game::InventorySystem inventory_system_;
    Game::CraftingSystem crafting_system_;
    Game::ExtractionSystem extraction_system_;
    Game::EconomyResultSystem economy_result_system_;
    Game::PvEAISystem pve_ai_system_;
    CoreEngine::SteamMultiplayerDebugPanel steam_multiplayer_debug_panel_;
    CoreEngine::World *world_ = nullptr;
    CoreEngine::NetworkSystem *network_system_ = nullptr;
    CoreEngine::NetworkReplicator *network_replicator_ = nullptr;
    CoreEngine::Node plane_node_;
    CoreEngine::Node secondary_camera_node_;
    CoreEngine::TextureHandle floor_texture_;
    CoreEngine::WindowCursorMode current_cursor_mode_ = CoreEngine::WindowCursorMode::CURSOR_NORMAL;
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
    config.renderBackend = CoreEngine::RenderBackendType::DiligentVulkan;
    config.vsync = false;
    config.enableImGui = true;

    return CoreEngine::RunEngine(std::move(app), config);
}
