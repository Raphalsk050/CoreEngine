#include "player.h"

#include "core/application/engine_context.h"
#include "core/application/frame_context.h"
#include "core/assets/model_asset.h"
#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/log/log.h"
#include "core/network/network_system.h"
#include "core/network/prediction/network_prediction_system.h"
#include "core/network/prediction/reconciliation.h"
#include "core/network/replication/network_replicator.h"
#include "core/network/replication/network_transform_component.h"
#include "core/network/replication/replicated_state_types.h"
#include "core/render/render_system.h"
#include "core/simulation/simulation_frame.h"

namespace Game {
    namespace {
        constexpr const char *kPlayerModelPath = "app/assets/models/mandalorian_armored.glb";

        [[nodiscard]] MovementComponent MakeDefaultMovementComponent() noexcept {
            return MovementComponent{
                .crouch_speed = 1.5f,
                .walk_speed = 3.0f,
                .run_speed = 6.0f,
                .default_movement_type = MovementType::Walk,
            };
        }
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

        prediction_system_ = &context.prediction_system;
        network_system_ = &context.network_system;
        const std::uint64_t local_user_id = context.online_system.Status().local_user_id;
        network_entity_id_ = context.network_replicator.RegisterEntity(
            player_pawn_.Node(),
            local_user_id != 0u ? local_user_id : 1u,
            CoreEngine::kInvalidPeerId,
            true);
        if (player_pawn_.Node().TryGetComponent<CoreEngine::PlayerMovementStateComponent>() == nullptr) {
            player_pawn_.Node().AddComponent<CoreEngine::PlayerMovementStateComponent>();
        }
        if (player_pawn_.Node().TryGetComponent<CoreEngine::HealthComponent>() == nullptr) {
            player_pawn_.Node().AddComponent<CoreEngine::HealthComponent>();
        }
        if (player_pawn_.Node().TryGetComponent<CoreEngine::ArmorSegmentsComponent>() == nullptr) {
            player_pawn_.Node().AddComponent<CoreEngine::ArmorSegmentsComponent>();
        }
        if (player_pawn_.Node().TryGetComponent<CoreEngine::InventoryComponent>() == nullptr) {
            player_pawn_.Node().AddComponent<CoreEngine::InventoryComponent>();
        }
        if (player_pawn_.Node().TryGetComponent<CoreEngine::EquipmentComponent>() == nullptr) {
            player_pawn_.Node().AddComponent<CoreEngine::EquipmentComponent>();
        }
        initialized_ = true;
        return input_bound;
    }

    void Player::Update(const CoreEngine::FrameContext &frame) {
        if (!initialized_) {
            return;
        }

        player_controller_.Update(frame);
    }

    void Player::FixedUpdate(const CoreEngine::SimulationFrame &frame) {
        if (!initialized_ || prediction_system_ == nullptr || network_system_ == nullptr) {
            return;
        }

        player_controller_.FixedUpdate(frame, *prediction_system_, *network_system_, network_entity_id_);

        if (network_system_->Session().Role() == CoreEngine::NetworkRole::Client) {
            auto *movement = player_pawn_.Node().TryGetComponent<CoreEngine::PlayerMovementStateComponent>();
            auto *network_transform = player_pawn_.Node().TryGetComponent<CoreEngine::NetworkTransformComponent>();
            auto *transform = player_pawn_.Node().TryGetComponent<CoreEngine::TransformComponent>();
            if (movement != nullptr &&
                network_transform != nullptr &&
                transform != nullptr &&
                movement->last_processed_input_sequence > last_reconciled_input_sequence_) {
                const CoreEngine::PredictedMovementState authoritative_state{
                    .position = network_transform->authoritative_position,
                    .rotation = network_transform->authoritative_rotation,
                    .velocity = movement->velocity,
                    .movement_flags = 0,
                };
                const CoreEngine::ReconciliationResult result =
                    prediction_system_->Reconcile(authoritative_state, movement->last_processed_input_sequence);
                if (result.action == CoreEngine::ReconciliationAction::SmoothCorrection ||
                    result.action == CoreEngine::ReconciliationAction::HardSnap) {
                    transform->SetPosition(authoritative_state.position);
                    transform->SetRotation(authoritative_state.rotation);
                }
                last_reconciled_input_sequence_ = movement->last_processed_input_sequence;
            }
        }
    }

    void Player::Shutdown() {
        player_controller_.DetachCameraController();
        player_controller_.Unpossess();
        prediction_system_ = nullptr;
        network_system_ = nullptr;
        network_entity_id_ = 0;
        last_reconciled_input_sequence_ = 0;
        initialized_ = false;
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
