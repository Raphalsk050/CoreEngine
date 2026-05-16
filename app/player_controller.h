#pragma once

#include "i_controller.h"
#include "player_command.h"
#include "third_person_camera_controller.h"
#include "core/network/prediction/player_input_command.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
    struct EngineContext;
    struct SimulationFrame;
    class NetworkPredictionSystem;
    class NetworkSystem;
}

namespace Game {
    class PlayerPawn;

    class PlayerController final : public IController {
    public:
        PlayerController() = default;

        [[nodiscard]] bool Init(const CoreEngine::EngineContext &context);

        void Update(const CoreEngine::FrameContext &frame) override;

        void FixedUpdate(const CoreEngine::SimulationFrame &frame,
                         CoreEngine::NetworkPredictionSystem &prediction_system,
                         CoreEngine::NetworkSystem &network_system,
                         CoreEngine::NetworkEntityId local_network_id);

        void Possess(IPossessable &possessable) override;

        void PossessPlayerPawn(PlayerPawn &player_pawn);

        void Unpossess() override;

        void AttachCameraController(ThirdPersonCameraController &camera_controller) noexcept;

        void DetachCameraController() noexcept;

        [[nodiscard]] bool HasPossessable() const noexcept;

    private:
        [[nodiscard]] PlayerCommand BuildCommand(const CoreEngine::FrameContext &frame) const noexcept;

        [[nodiscard]] CoreEngine::PlayerInputCommand BuildInputCommand(
            const CoreEngine::SimulationFrame &frame,
            CoreEngine::NetworkPredictionSystem &prediction_system) const noexcept;

        [[nodiscard]] CoreEngine::Math::Vec2 BuildLookDelta(const CoreEngine::FrameContext &frame) const noexcept;

        void BuildCameraDistance(const CoreEngine::FrameContext &frame) const noexcept;

        IPossessable *possessable_ = nullptr;
        PlayerPawn *player_pawn_ = nullptr;
        ThirdPersonCameraController *camera_controller_ = nullptr;
        PlayerCommand latest_command_{};
        bool network_input_fire_held_ = false;
        bool network_input_reload_pressed_ = false;
        bool network_input_interact_pressed_ = false;
        bool network_input_capture_held_ = false;

        float mouse_sensitivity_x_ = 0.12f;
        float mouse_sensitivity_y_ = 0.12f;
        float max_camera_distance_ = 20.0f;
        float min_camera_distance_ = 3.0f;
        bool invert_y_ = false;
    };
} // namespace Game
