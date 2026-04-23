#pragma once

#include <glm/vec2.hpp>

namespace Game {
    struct PlayerCommand {
        glm::vec2 movement{0.0f, 0.0f};
        bool jump_pressed = false;
        bool run_held = false;
    };
} // namespace Game