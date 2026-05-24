#pragma once

#include <cstdint>

#include "core/ecs/components/transform_component.h"
#include "core/network/prediction/player_input_command.h"
#include "core/network/prediction/prediction_buffer.h"

namespace CoreEngine {
    enum class NetworkedPlayerMovementType : std::uint8_t {
        Crouch = 0,
        Walk,
        Run,
    };

    struct NetworkedPlayerMovementComponent {
        float crouch_speed = 1.5f;
        float walk_speed = 3.0f;
        float run_speed = 6.0f;
        NetworkedPlayerMovementType default_movement_type = NetworkedPlayerMovementType::Walk;
        PlayerCommandActionId sprint_action{};
    };

    /**
     * @brief Applies network player movement commands to fixed-tick transform state.
     *
     * Responsibility: keep client prediction, host authority, and reconciliation
     * replay on one deterministic movement path owned by the engine.
     */
    class NetworkedPlayerMovementSimulation final {
    public:
        static void ApplyInputCommand(TransformComponent &transform,
                                      const NetworkedPlayerMovementComponent &movement,
                                      const PlayerInputCommand &command,
                                      float fixed_delta_time) noexcept;

        [[nodiscard]] static PredictedMovementState BuildMovementState(
            const TransformComponent &transform) noexcept;

    private:
        [[nodiscard]] static float ResolveSpeed(const NetworkedPlayerMovementComponent &movement,
                                                const PlayerInputCommand &command) noexcept;

        static void RotateThroughMovement(float fixed_delta_time,
                                          TransformComponent &transform,
                                          Math::Vec3 move) noexcept;
    };
} // namespace CoreEngine
