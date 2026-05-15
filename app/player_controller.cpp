#include "player_controller.h"

#include "player_pawn.h"

#include "core/application/engine_context.h"
#include "core/application/frame_context.h"
#include "core/debug/debug.h"
#include "core/input/input_system.h"
#include "core/log/log.h"
#include "core/network/network_system.h"
#include "core/network/prediction/network_prediction_system.h"
#include "core/simulation/simulation_frame.h"

namespace Game {
    namespace Actions {
        constexpr CoreEngine::InputActionId Move = CoreEngine::MakeInputActionId(1);
        constexpr CoreEngine::InputActionId Jump = CoreEngine::MakeInputActionId(2);
        constexpr CoreEngine::InputActionId Run = CoreEngine::MakeInputActionId(3);
    }

    bool PlayerController::Init(const CoreEngine::EngineContext &context) {
        const bool move_bound = context.input_system.BindAxis2D(
            Actions::Move,
            CoreEngine::Key::A,
            CoreEngine::Key::D,
            CoreEngine::Key::S,
            CoreEngine::Key::W
        );

        const bool jump_bound = context.input_system.BindButton(Actions::Jump, CoreEngine::Key::Space);
        const bool run_bound = context.input_system.BindButton(Actions::Run, CoreEngine::Key::LeftShift);

        return move_bound && jump_bound && run_bound;
    }

    void PlayerController::Update(const CoreEngine::FrameContext &frame) {
        if (camera_controller_ != nullptr) {
            camera_controller_->ApplyLookDelta(BuildLookDelta(frame));
            BuildCameraDistance(frame);
        }

        latest_command_ = BuildCommand(frame);

        if (camera_controller_ != nullptr) {
            camera_controller_->Update(frame.delta_time);
        }
    }

    void PlayerController::FixedUpdate(const CoreEngine::SimulationFrame &frame,
                                       CoreEngine::NetworkPredictionSystem &prediction_system,
                                       CoreEngine::NetworkSystem &network_system) {
        if (player_pawn_ == nullptr) {
            return;
        }

        CoreEngine::PlayerInputCommand command = BuildInputCommand(frame, prediction_system);
        const CoreEngine::NetworkRole role = network_system.Session().Role();

        if (role == CoreEngine::NetworkRole::Client) {
            player_pawn_->ApplyPlayerInputCommand(command, frame.fixed_delta_time);
            prediction_system.RecordPrediction(command, player_pawn_->BuildMovementState());
            network_system.SendPlayerInputCommands(prediction_system.BuildRedundantCommandBatch(command));
            return;
        }

        player_pawn_->ApplyPlayerInputCommand(command, frame.fixed_delta_time);
    }

    void PlayerController::Possess(IPossessable &possessable) {
        if (possessable_ == &possessable) {
            return;
        }

        Unpossess();
        possessable_ = &possessable;
        possessable_->OnPossessed();
    }

    void PlayerController::PossessPlayerPawn(PlayerPawn &player_pawn) {
        if (possessable_ == &player_pawn) {
            return;
        }

        Unpossess();
        possessable_ = &player_pawn;
        player_pawn_ = &player_pawn;
        possessable_->OnPossessed();
    }

    void PlayerController::Unpossess() {
        if (possessable_ == nullptr) {
            return;
        }

        possessable_->OnUnpossessed();
        possessable_ = nullptr;
        player_pawn_ = nullptr;
    }

    void PlayerController::AttachCameraController(ThirdPersonCameraController &camera_controller) noexcept {
        camera_controller_ = &camera_controller;
    }

    void PlayerController::DetachCameraController() noexcept {
        camera_controller_ = nullptr;
    }

    bool PlayerController::HasPossessable() const noexcept {
        return possessable_ != nullptr;
    }

    PlayerCommand PlayerController::BuildCommand(const CoreEngine::FrameContext &frame) const noexcept {
        const auto [input_x, input_y] = frame.input_system.GetAxis2D(Actions::Move);
        const CoreEngine::Math::Vec2 movement_input{input_x, input_y};

        CoreEngine::Math::Vec3 world_move{movement_input.x, 0.0f, movement_input.y};

        if (camera_controller_ != nullptr) {
            const CoreEngine::Math::Vec3 forward = camera_controller_->PlanarForward();
            const CoreEngine::Math::Vec3 right = camera_controller_->PlanarRight();
            world_move = right * movement_input.x + forward * movement_input.y;
        }

        const float length_squared = CoreEngine::Math::Dot(world_move, world_move);
        if (length_squared > 1.0f) {
            world_move = CoreEngine::Math::Normalize(world_move);
        }

        return PlayerCommand{
            .movement_input = movement_input,
            .world_move = world_move,
            .jump_pressed = frame.input_system.WasActionPressed(Actions::Jump),
            .run_held = frame.input_system.IsActionDown(Actions::Run),
        };
    }

    CoreEngine::PlayerInputCommand PlayerController::BuildInputCommand(
        const CoreEngine::SimulationFrame &frame,
        CoreEngine::NetworkPredictionSystem &prediction_system) const noexcept {
        std::uint32_t buttons = 0;
        if (latest_command_.jump_pressed) {
            buttons |= static_cast<std::uint32_t>(CoreEngine::PlayerInputButton::Jump);
        }
        if (latest_command_.run_held) {
            buttons |= static_cast<std::uint32_t>(CoreEngine::PlayerInputButton::Sprint);
        }

        return CoreEngine::PlayerInputCommand{
            .client_tick = frame.tick,
            .sequence = prediction_system.NextSequence(),
            .last_received_server_snapshot_tick = 0,
            .move_x = latest_command_.movement_input.x,
            .move_y = latest_command_.movement_input.y,
            .look_yaw = camera_controller_ != nullptr ? camera_controller_->YawRadians() : 0.0f,
            .look_pitch = camera_controller_ != nullptr ? camera_controller_->PitchRadians() : 0.0f,
            .buttons = buttons,
            .selected_slot = 0,
        };
    }

    CoreEngine::Math::Vec2 PlayerController::BuildLookDelta(const CoreEngine::FrameContext &frame) const noexcept {
        const auto [mouse_x, mouse_y] = frame.input_system.MouseDelta();
        const float y_sign = invert_y_ ? -1.0f : 1.0f;

        return CoreEngine::Math::Vec2{
            mouse_x * mouse_sensitivity_x_,
            mouse_y * mouse_sensitivity_y_ * y_sign,
        };
    }

    void PlayerController::BuildCameraDistance(const CoreEngine::FrameContext &frame) const noexcept {
        float new_distance = CoreEngine::Math::Clamp(
            camera_controller_->GetDistance() - frame.input_system.MouseWheel().y, min_camera_distance_,
            max_camera_distance_);
        camera_controller_->SetDistance(new_distance);
    }
} // namespace Game
