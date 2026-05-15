#pragma once

#include "movement_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/network/prediction/player_input_command.h"
#include "core/network/prediction/prediction_buffer.h"

namespace Game {
    /**
     * @brief Applies fixed-tick player movement commands to transform state.
     *
     * Responsibility: keep client prediction and server-authoritative movement
     * on the same code path so reconciliation compares equivalent simulation.
     */
    class PlayerMovementSimulation final {
    public:
        static void ApplyInputCommand(CoreEngine::TransformComponent &transform,
                                      const MovementComponent &movement,
                                      const CoreEngine::PlayerInputCommand &command,
                                      float fixed_delta_time) noexcept;

        [[nodiscard]] static CoreEngine::PredictedMovementState BuildMovementState(
            const CoreEngine::TransformComponent &transform) noexcept;

    private:
        [[nodiscard]] static float ResolveSpeed(const MovementComponent &movement,
                                                const CoreEngine::PlayerInputCommand &command) noexcept;

        static void RotateThroughMovement(float fixed_delta_time,
                                          CoreEngine::TransformComponent &transform,
                                          CoreEngine::Math::Vec3 move) noexcept;
    };
} // namespace Game
