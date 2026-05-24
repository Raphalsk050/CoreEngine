#include "player_controller.h"

#include "core/application/engine_context.h"
#include "core/application/frame_context.h"
#include "core/debug/debug_draw.h"
#include "game/input/player_input_bindings.h"

namespace Game {
    bool PlayerController::Initialize(const CoreEngine::EngineContext &context) noexcept {
        return PlayerInputBindings::BindDefaults(context.input_system);
    }

    void PlayerController::Update(const CoreEngine::FrameContext &frame) noexcept {
        if (camera_controller_ != nullptr) {
            camera_controller_->ApplyLookDeltaRadians(BuildLookDeltaRadians(frame));
            camera_controller_->AdjustDistance(-frame.input_system.MouseWheel().y);
        }

        frame.network_players.SetLocalInput(BuildNetworkInput(frame));

        if (camera_controller_ != nullptr) {
            camera_controller_->Update(frame.delta_time);
        }
    }

    void PlayerController::AttachCameraController(ThirdPersonCameraController &camera_controller) noexcept {
        camera_controller_ = &camera_controller;
    }

    void PlayerController::DetachCameraController() noexcept {
        camera_controller_ = nullptr;
    }

    CoreEngine::NetworkPlayerInputState PlayerController::BuildNetworkInput(
        const CoreEngine::FrameContext &frame) const noexcept {
        const CoreEngine::InputVector2 move_axis = frame.input_system.GetAxis2D(PlayerInputActions::Move);
        CoreEngine::Math::Vec2 movement{move_axis.x, move_axis.y};
        if (CoreEngine::Math::LengthSquared(movement) > 1.0f) {
            movement = CoreEngine::Math::Normalize(movement);
        }

        CoreEngine::NetworkPlayerInputState input;
        input.movement = movement;
        input.look_yaw = camera_controller_ != nullptr ? camera_controller_->YawRadians() : 0.0f;
        input.look_pitch = camera_controller_ != nullptr ? camera_controller_->PitchRadians() : 0.0f;
        input.selected_slot = 0;
        input.SetAction(PlayerCommandActions::Jump,
                        frame.input_system.WasActionPressed(PlayerInputActions::Jump));
        input.SetAction(PlayerCommandActions::Crouch,
                        frame.input_system.IsActionDown(PlayerInputActions::Crouch));
        input.SetAction(PlayerCommandActions::Sprint,
                        frame.input_system.IsActionDown(PlayerInputActions::Sprint));
        input.SetAction(PlayerCommandActions::Fire,
                        frame.input_system.IsActionDown(PlayerInputActions::Fire));
        input.SetAction(PlayerCommandActions::AltFire,
                        frame.input_system.IsActionDown(PlayerInputActions::AltFire));
        input.SetAction(PlayerCommandActions::Reload,
                        frame.input_system.WasActionPressed(PlayerInputActions::Reload));
        input.SetAction(PlayerCommandActions::Interact,
                        frame.input_system.WasActionPressed(PlayerInputActions::Interact));
        input.SetAction(PlayerCommandActions::UseGadget,
                        frame.input_system.WasActionPressed(PlayerInputActions::UseGadget));
        input.SetAction(PlayerCommandActions::Capture,
                        frame.input_system.IsActionDown(PlayerInputActions::Capture));
        return input;
    }

    CoreEngine::Math::Vec2 PlayerController::BuildLookDeltaRadians(
        const CoreEngine::FrameContext &frame) const noexcept {
        const CoreEngine::InputVector2 mouse_delta = frame.input_system.MouseDelta();
        const float y_sign = invert_y_ ? -1.0f : 1.0f;
        return {
            mouse_delta.x * mouse_sensitivity_x_,
            mouse_delta.y * mouse_sensitivity_y_ * y_sign,
        };
    }
} // Game
