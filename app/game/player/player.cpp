#include "player.h"

#include "core/application/engine_context.h"
#include "core/application/frame_context.h"
#include "core/debug/debug_draw.h"
#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/spring_arm_component.h"
#include "core/ecs/world.h"
#include "core/log/log.h"
#include "core/network/player/network_player_ids.h"
#include "core/network/replication/replicated_state_types.h"
#include "core/render/render_system.h"

namespace Game {
    namespace {
        constexpr const char *kPlayerModelPath = "app/assets/models/mandalorian_armored.glb";
    }

    CoreEngine::NetworkedPlayerMovementComponent Player::MakeDefaultMovement() noexcept {
        return CoreEngine::NetworkedPlayerMovementComponent{
            .crouch_speed = 1.5f,
            .walk_speed = 3.0f,
            .run_speed = 6.0f,
            .default_movement_type = CoreEngine::NetworkedPlayerMovementType::Walk,
        };
    }

    bool Player::Initialize(const CoreEngine::EngineContext &context) {
        Shutdown();

        render_system_ = &context.render_system;
        network_players_ = &context.network_players;

        const bool input_bound = controller_.Initialize(context);
        if (!input_bound) {
            CoreEngine::Log::Warn("Game", "One or more player input bindings failed");
        }

        CreateLocalPlayer(context.world);
        CreateCamera(context.world);
        controller_.AttachCameraController(camera_controller_);
        LoadPlayerModel(context);

        const bool network_configured = context.network_players.Configure(CoreEngine::NetworkPlayerSystemDesc{
            .archetype_id = CoreEngine::kDefaultNetworkPlayerArchetypeId,
            .presentation_id = CoreEngine::kDefaultNetworkPlayerPresentationId,
            .movement = MakeDefaultMovement(),
            .initialize_player_entity = &Player::InitializeNetworkPlayerEntity,
            .user_data = this,
        });
        if (!network_configured) {
            CoreEngine::Log::Error("Game", "Failed to configure NetworkPlayerSystem");
        }

        const CoreEngine::NetworkEntityId local_network_id =
            context.network_players.RegisterLocalPlayer(CoreEngine::LocalNetworkPlayerDesc{
                .node = player_node_,
                .local_user_id = context.online_system.Status().local_user_id,
                .presentation_id = CoreEngine::kDefaultNetworkPlayerPresentationId,
                .movement = MakeDefaultMovement(),
            });
        if (local_network_id == 0u) {
            CoreEngine::Log::Error("Game", "Failed to register local player for multiplayer");
        }

        initialized_ = player_node_.IsValid() && local_network_id != 0u;
        return initialized_ && input_bound && network_configured;
    }

    void Player::Update(const CoreEngine::FrameContext &frame) {
        if (!initialized_) {
            return;
        }

        controller_.Update(frame);

        if (player_node_.IsValid()) {
            frame.debug_draw.DrawSphere(player_node_.GetPosition(), 0.2f);
        }
    }

    void Player::Shutdown() {
        controller_.DetachCameraController();

        if (network_players_ != nullptr) {
            network_players_->ClearLocalPlayer();
        }

        if (camera_node_.IsValid()) {
            camera_node_.Destroy();
        }
        if (player_node_.IsValid()) {
            player_node_.Destroy();
        }

        player_node_ = {};
        player_renderer_node_ = {};
        player_model_root_ = {};
        camera_node_ = {};
        network_players_ = nullptr;
        render_system_ = nullptr;
        player_model_ = {};
        initialized_ = false;
    }

    void Player::InitializeNetworkPlayerEntity(CoreEngine::NetworkPlayerEntityInitContext &context,
                                               void *user_data) {
        auto *player = static_cast<Player *>(user_data);
        if (player == nullptr) {
            return;
        }

        player->ConfigureNetworkPlayerEntity(context);
    }

    void Player::ConfigureNetworkPlayerEntity(CoreEngine::NetworkPlayerEntityInitContext &context) {
        if (!context.node.IsValid()) {
            return;
        }

        if (context.node.TryGetComponent<CoreEngine::HealthComponent>() == nullptr) {
            context.node.AddComponent<CoreEngine::HealthComponent>();
        }
        if (context.node.TryGetComponent<CoreEngine::ArmorSegmentsComponent>() == nullptr) {
            context.node.AddComponent<CoreEngine::ArmorSegmentsComponent>();
        }
        if (context.node.TryGetComponent<CoreEngine::InventoryComponent>() == nullptr) {
            context.node.AddComponent<CoreEngine::InventoryComponent>();
        }
        if (context.node.TryGetComponent<CoreEngine::EquipmentComponent>() == nullptr) {
            context.node.AddComponent<CoreEngine::EquipmentComponent>();
        }

        if (!context.local_player && context.presentation_id == CoreEngine::kDefaultNetworkPlayerPresentationId) {
            CoreEngine::Node renderer_node = context.world.CreateNode("RemotePlayerRenderer");
            renderer_node.SetParent(context.node);
            renderer_node.RotateEuler({-90.0f, 180.0f, 0.0f});
            renderer_node.SetPosition(renderer_node.GetPosition() + CoreEngine::Math::Vec3{0.0f, -0.75f, 0.0f});
            renderer_node.SetScale({0.01f, 0.01f, 0.01f});
            AttachPlayerModel(context.world, renderer_node, "RemotePlayerModel", nullptr);
        }
    }

    void Player::CreateLocalPlayer(CoreEngine::World &world) {
        player_node_ = world.CreateNode("LocalPlayer");
        player_node_.SetPosition({2.0f, 0.0f, 0.0f});

        player_renderer_node_ = world.CreateNode("LocalPlayerRenderer");
        player_renderer_node_.SetParent(player_node_);
        player_renderer_node_.RotateEuler({-90.0f, 180.0f, 0.0f});
        player_renderer_node_.SetPosition(player_renderer_node_.GetPosition() + CoreEngine::Math::Vec3{0.0f, -0.75f, 0.0f});
        player_renderer_node_.SetScale({0.01f, 0.01f, 0.01f});
    }

    void Player::CreateCamera(CoreEngine::World &world) {
        camera_node_ = world.CreateNode("MainCamera");
        camera_node_.AddComponent<CoreEngine::CameraComponent>(CoreEngine::CameraComponent{
            .projection_type = CoreEngine::CameraProjectionType::Perspective,
            .aspect_mode = CoreEngine::CameraAspectMode::RenderSurface,
            .fov_y_degrees = 65.0f,
            .near_z = 0.03f,
            .far_z = 1000.0f,
            .priority = 100,
            .enabled = true,
        });

        CoreEngine::SpringArmComponent spring_arm;
        spring_arm.rest_length = 4.0f;
        spring_arm.pivot_offset_local = {0.0f, 1.35f, 0.0f};
        spring_arm.orbit_pitch_radians = CoreEngine::Math::Deg2Rad(-15.0f);
        spring_arm.smoothing.position_enabled = true;
        spring_arm.smoothing.rotation_enabled = true;
        spring_arm.smoothing.position_follow_speed = 16.0f;
        spring_arm.smoothing.rotation_follow_speed = 20.0f;
        spring_arm.obstruction.enabled = false;
        camera_node_.AddComponent<CoreEngine::SpringArmComponent>(spring_arm);

        (void) camera_controller_.Attach(camera_node_, player_node_);
        camera_controller_.SetDistanceLimits(2.0f, 12.0f);
        camera_controller_.Update(0.0f);
    }

    void Player::LoadPlayerModel(const CoreEngine::EngineContext &context) {
        player_model_ = context.render_system.LoadModel(CoreEngine::ModelLoadDesc{
            .path = kPlayerModelPath,
        });
        if (!player_model_.IsValid()) {
            CoreEngine::Log::Error("Game", "Failed to load player model: {}", kPlayerModelPath);
            return;
        }

        AttachPlayerModel(context.world, player_renderer_node_, "LocalPlayerModel", &player_model_root_);
    }

    void Player::AttachPlayerModel(CoreEngine::World &world,
                                   CoreEngine::Node parent,
                                   const char *root_name,
                                   CoreEngine::Node *out_root) {
        if (render_system_ == nullptr || !player_model_.IsValid() || !parent.IsValid()) {
            return;
        }

        CoreEngine::ModelInstance instance = render_system_->InstantiateModel(
            world,
            player_model_,
            parent,
            CoreEngine::ModelInstantiationDesc{
                .root_name = root_name != nullptr ? root_name : "PlayerModel",
            });
        if (!instance.IsValid() || instance.mesh_nodes.empty()) {
            CoreEngine::Log::Error("Game", "Player model instantiated without renderable nodes");
            return;
        }

        if (out_root != nullptr) {
            *out_root = instance.root;
        }
    }
} // Game
