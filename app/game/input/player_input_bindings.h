#pragma once

#include "core/input/input_system.h"

namespace Game {
    struct PlayerInputActions {
        static constexpr CoreEngine::InputActionId Move = CoreEngine::MakeInputActionId(1);
        static constexpr CoreEngine::InputActionId Jump = CoreEngine::MakeInputActionId(2);
        static constexpr CoreEngine::InputActionId Sprint = CoreEngine::MakeInputActionId(3);
        static constexpr CoreEngine::InputActionId Crouch = CoreEngine::MakeInputActionId(4);
        static constexpr CoreEngine::InputActionId Fire = CoreEngine::MakeInputActionId(5);
        static constexpr CoreEngine::InputActionId AltFire = CoreEngine::MakeInputActionId(6);
        static constexpr CoreEngine::InputActionId Reload = CoreEngine::MakeInputActionId(7);
        static constexpr CoreEngine::InputActionId Interact = CoreEngine::MakeInputActionId(8);
        static constexpr CoreEngine::InputActionId UseGadget = CoreEngine::MakeInputActionId(9);
        static constexpr CoreEngine::InputActionId Capture = CoreEngine::MakeInputActionId(10);
    };

    /**
     * @brief Owns default keyboard and mouse bindings for the local player.
     *
     * Responsibility: keep gameplay input action IDs and physical key bindings
     * in one app-facing module so controllers only read semantic actions.
     */
    class PlayerInputBindings final {
    public:
        [[nodiscard]] static bool BindDefaults(CoreEngine::InputSystem &input_system) noexcept;
    };
} // namespace Game
