#include "player_controller.h"

#include <string>

#include "core/application/engine_context.h"
#include "core/application/frame_context.h"
#include "core/input/input_system.h"
#include "core/log/log.h"

namespace Game {
    namespace Actions {
        constexpr CoreEngine::InputActionId Move = CoreEngine::MakeInputActionId(1);
        constexpr CoreEngine::InputActionId Jump = CoreEngine::MakeInputActionId(2);
    }

    void PlayerController::Init(const CoreEngine::EngineContext &context) {
        bool success = context.input_system.BindAxis2D(
            Actions::Move,
            CoreEngine::Key::A,
            CoreEngine::Key::D,
            CoreEngine::Key::S,
            CoreEngine::Key::W
        );

        context.input_system.BindButton(Actions::Jump, CoreEngine::Key::Space);
    }

    void PlayerController::ProcessEvents(const CoreEngine::FrameContext &frame) {
        CoreEngine::InputVector2 move = frame.input_system.GetAxis2D(Actions::Move);

        CoreEngine::Log::Error("Game", "Input: {" + std::to_string(move.x) + ", " + std::to_string(move.y) + "}");

        if (frame.input_system.WasActionPressed(Actions::Jump)) {
            CoreEngine::Log::Error("Game", "Player jumped");
        }

        if (frame.input_system.WasActionPressed(Actions::Move)) {
            CoreEngine::Log::Error(
                "Game",
                "On pressed move action Input: {" + std::to_string(move.x) + ", " + std::to_string(move.y) + "}");
        }

        // IsKeyDown(...) // segurando
        // WasKeyPressed(...) // apertou neste frame
        // WasKeyReleased(...) // soltou neste frame
        // MouseDelta() // movimento acumulado do mouse neste frame
        // MouseWheel() // scroll acumulado neste frame
    }

    void PlayerController::Possess(IPossessable &possessable) {
        possessable_ = possessable;
        possessable_.Possess();
    }

    void PlayerController::Unpossess() {
        possessable_.UnPossess();
    }
}
