#include "player_input_bindings.h"

namespace Game {
    bool PlayerInputBindings::BindDefaults(CoreEngine::InputSystem &input_system) noexcept {
        const bool move_bound = input_system.BindAxis2D(
            PlayerInputActions::Move,
            CoreEngine::Key::A,
            CoreEngine::Key::D,
            CoreEngine::Key::S,
            CoreEngine::Key::W);

        const bool jump_bound = input_system.BindButton(PlayerInputActions::Jump, CoreEngine::Key::Space);
        const bool sprint_bound = input_system.BindButton(PlayerInputActions::Sprint, CoreEngine::Key::LeftShift);
        const bool crouch_bound = input_system.BindButton(PlayerInputActions::Crouch, CoreEngine::Key::LeftControl);
        const bool fire_bound = input_system.BindButton(PlayerInputActions::Fire, CoreEngine::MouseButton::Left);
        const bool alt_fire_bound = input_system.BindButton(PlayerInputActions::AltFire, CoreEngine::MouseButton::Right);
        const bool reload_bound = input_system.BindButton(PlayerInputActions::Reload, CoreEngine::Key::R);
        const bool interact_bound = input_system.BindButton(PlayerInputActions::Interact, CoreEngine::Key::E);
        const bool gadget_bound = input_system.BindButton(PlayerInputActions::UseGadget, CoreEngine::Key::Q);
        const bool capture_bound = input_system.BindButton(PlayerInputActions::Capture, CoreEngine::Key::F);

        return move_bound &&
               jump_bound &&
               sprint_bound &&
               crouch_bound &&
               fire_bound &&
               alt_fire_bound &&
               reload_bound &&
               interact_bound &&
               gadget_bound &&
               capture_bound;
    }
} // namespace Game
