#include "player_controller.h"

#include "core/application/engine_context.h"
#include "core/application/frame_context.h"
#include "core/debug/debug.h"
#include "core/input/input_system.h"
#include "core/log/log.h"

namespace Game {
    namespace Actions {
        constexpr CoreEngine::InputActionId Move = CoreEngine::MakeInputActionId(1);
        constexpr CoreEngine::InputActionId Jump = CoreEngine::MakeInputActionId(2);
        constexpr CoreEngine::InputActionId Run = CoreEngine::MakeInputActionId(3);
        constexpr CoreEngine::InputActionId Crouch = CoreEngine::MakeInputActionId(4);
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
        if (possessable_ == nullptr) {
            return;
        }

        possessable_->ApplyPlayerCommand(BuildCommand(frame), frame.delta_time);
    }

    void PlayerController::Possess(IPossessable &possessable) {
        if (possessable_ == &possessable) {
            return;
        }

        Unpossess();
        possessable_ = &possessable;
        possessable_->OnPossessed();
    }

    void PlayerController::Unpossess() {
        if (possessable_ == nullptr) {
            return;
        }

        possessable_->OnUnpossessed();
        possessable_ = nullptr;
    }

    bool PlayerController::HasPossessable() const noexcept {
        return possessable_ != nullptr;
    }

    PlayerCommand PlayerController::BuildCommand(const CoreEngine::FrameContext &frame) noexcept {
        const auto [input_x, input_y] = frame.input_system.GetAxis2D(Actions::Move);
        const auto [mouse_x, mouse_y] = frame.input_system.MouseDelta();
        const auto mouse_input = frame.input_system.MouseDelta();

        // CoreEngine::Log::Info(
        //     "Game", "Mouse input is: {" + std::to_string(mouse_x) + "," + std::to_string(mouse_y) + "}");

        return PlayerCommand{
            .movement = CoreEngine::Math::Vec2{input_x, input_y},
            .look = CoreEngine::Math::Vec2{mouse_x, mouse_y},
            .jump_pressed = frame.input_system.WasActionPressed(Actions::Jump),
            .run_held = frame.input_system.IsActionDown(Actions::Run),
        };
    }
} // namespace Game
