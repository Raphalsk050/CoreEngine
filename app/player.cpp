#include "player.h"

#include "core/application/engine_context.h"
#include "core/application/frame_context.h"
#include "core/assets/model_asset.h"
#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/log/log.h"
#include "core/network/player/network_player_ids.h"
#include "core/network/replication/replicated_state_types.h"
#include "core/render/render_system.h"

namespace Game {
    namespace {
        constexpr const char *kPlayerModelPath = "app/assets/models/mandalorian_armored.glb";
    }

    CoreEngine::NetworkedPlayerMovementComponent Player::MakeDefaultMovementComponent() noexcept {
        return CoreEngine::NetworkedPlayerMovementComponent{
            .crouch_speed = 1.5f,
            .walk_speed = 3.0f,
            .run_speed = 6.0f,
            .default_movement_type = CoreEngine::NetworkedPlayerMovementType::Walk,
        };
    }

    bool Player::Initialize(const CoreEngine::EngineContext &context) {
        const bool input_bound = player_controller_.Init(context);
        if (!input_bound) {
            CoreEngine::Log::Warn("Game", "Failed to bind one or more player input actions");
        }

        CreatePawn(context.world);
        LoadPlayerModel(context);

        CreateCamera(context.world);
        AttachController();

        render_system_ = &context.render_system;
        network_players_ = &context.network_players;
        if (!context.network_players.Configure(CoreEngine::NetworkPlayerSystemDesc{
                .archetype_id = CoreEngine::kDefaultNetworkPlayerArchetypeId,
                .presentation_id = CoreEngine::kDefaultNetworkPlayerPresentationId,
                .movement = MakeDefaultMovementComponent(),
                .initialize_player_entity = &Player::InitializeNetworkPlayerEntity,
                .user_data = this,
            })) {
            CoreEngine::Log::Error("Game", "Failed to configure engine network player system");
        }

        const std::uint64_t local_user_id = context.online_system.Status().local_user_id;
        (void) context.network_players.RegisterLocalPlayer(CoreEngine::LocalNetworkPlayerDesc{
            .node = player_pawn_.Node(),
            .local_user_id = local_user_id,
            .presentation_id = CoreEngine::kDefaultNetworkPlayerPresentationId,
            .movement = MakeDefaultMovementComponent(),
        });
        initialized_ = true;
        return input_bound;
    }

    void Player::Update(const CoreEngine::FrameContext &frame) {
        if (!initialized_) {
            return;
        }

        player_controller_.Update(frame);
    }

    void Player::Shutdown() {
        player_controller_.DetachCameraController();
        player_controller_.Unpossess();
        if (network_players_ != nullptr) {
            network_players_->ClearLocalPlayer();
        }
        network_players_ = nullptr;
        render_system_ = nullptr;
        initialized_ = false;
    }

    void Player::InitializeNetworkPlayerEntity(CoreEngine::NetworkPlayerEntityInitContext &context,
                                               void *user_data) {
        auto *player = static_cast<Player *>(user_data);
        if (player == nullptr) {
            return;
        }

        player->ConfigureNetworkPlayerNode(context);
    }

    void Player::ConfigureNetworkPlayerNode(CoreEngine::NetworkPlayerEntityInitContext &context) {
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

        if (!context.local_player &&
            context.presentation_id == CoreEngine::kDefaultNetworkPlayerPresentationId) {
            AttachRemotePlayerModel(context.world, context.node);
        }
    }

    void Player::AttachRemotePlayerModel(CoreEngine::World &world, CoreEngine::Node player_node) {
        if (render_system_ == nullptr || !player_node.IsValid()) {
            return;
        }

        CoreEngine::Node renderer_node = world.CreateNode("RemotePlayerRenderer");
        renderer_node.SetParent(player_node);
        renderer_node.RotateEuler({-90.0f, 180.0f, 0.0f});
        renderer_node.SetPosition(renderer_node.GetPosition() + CoreEngine::Math::Vec3(0.0f, -0.75f, 0.0f));
        renderer_node.SetScale(CoreEngine::Math::Vec3(0.01f));

        CoreEngine::RenderSystem *render_system = render_system_;
        CoreEngine::Future<CoreEngine::ModelHandle> model = render_system->LoadModelAsyncFuture(
            CoreEngine::ModelLoadDesc{
                .path = kPlayerModelPath,
            });

        model.Then([render_system, renderer_node, &world](
        const CoreEngine::FutureResult<CoreEngine::ModelHandle> &result) {
                if (!result.IsSuccess()) {
                    CoreEngine::Log::Error("Game", "Failed to load remote player model: {}", result.ErrorMessage());
                    return;
                }

                CoreEngine::ModelInstance instance = render_system->InstantiateModel(
                    world,
                    result.Value(),
                    renderer_node,
                    CoreEngine::ModelInstantiationDesc{
                        .root_name = "RemotePlayerModel",
                    });

                if (!instance.IsValid() || instance.mesh_nodes.empty()) {
                    CoreEngine::Log::Error("Game", "Remote player model loaded without renderable nodes");
                }
            });
    }

    void Player::CreatePawn(CoreEngine::World &world) {
        player_pawn_ = PlayerPawn(world.CreateNode("Player"), MakeDefaultMovementComponent());
        player_pawn_.Node().SetPosition(CoreEngine::Math::Vec3(2.0f, 0.0f, 0.0f));
        player_renderer_node_ = world.CreateNode("PlayerRendererNode");
        player_renderer_node_.SetParent(player_pawn_.Node());

        player_renderer_node_.RotateEuler({-90.0, 180.0f, 0.0});

        player_renderer_node_.
                SetPosition(player_renderer_node_.GetPosition() + CoreEngine::Math::Vec3(0.0f, -0.75f, 0.0f));

        player_renderer_node_.
                SetScale(CoreEngine::Math::Vec3(0.01f));
    }

    void Player::CreateCamera(CoreEngine::World &world) {
        camera_node_ = world.CreateNode("MainCamera");
        camera_node_.AddComponent<CoreEngine::CameraComponent>();
        camera_node_.SetPosition({0.0f, 1.5f, -4.0f});

        third_person_camera_controller_.Attach(camera_node_, player_pawn_.Node());
        third_person_camera_controller_.SetFocusOffset({0.0f, 1.25f, 0.0f});
        third_person_camera_controller_.SetDistance(4.0f);
    }

    void Player::LoadPlayerModel(const CoreEngine::EngineContext &context) {
        CoreEngine::Future<CoreEngine::ModelHandle> model = context.render_system.LoadModelAsyncFuture(
            CoreEngine::ModelLoadDesc{
                .path = kPlayerModelPath,
            });

        model.Then([this, &world = context.world, &render_system = context.render_system](
        const CoreEngine::FutureResult<CoreEngine::ModelHandle> &result) {
                if (!result.IsSuccess()) {
                    CoreEngine::Log::Error("Game", "Failed to load player model: {}", result.ErrorMessage());
                    return;
                }

                CoreEngine::ModelInstance instance = render_system.InstantiateModel(
                    world,
                    result.Value(),
                    player_renderer_node_,
                    CoreEngine::ModelInstantiationDesc{
                        .root_name = "PlayerModel",
                    });

                if (!instance.IsValid() || instance.mesh_nodes.empty()) {
                    CoreEngine::Log::Error("Game", "Player model loaded without renderable nodes");
                    return;
                }

                player_model_root_ = instance.root;
            });
    }

    void Player::AttachController() {
        player_controller_.AttachCameraController(third_person_camera_controller_);
        player_controller_.PossessPlayerPawn(player_pawn_);
    }
} // namespace Game
