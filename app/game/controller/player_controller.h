#pragma once

#include "game/camera/third_person_camera_controller.h"
#include "core/math/math.h"
#include "core/network/player/network_player_system.h"

namespace CoreEngine {
    struct EngineContext;
    struct FrameContext;
}

namespace Game {
    /**
     * @brief Translates local keyboard and mouse state into network player input.
     *
     * Responsibility: own player input bindings, camera look control, and
     * submission of semantic player intent to the engine multiplayer path.
     */
    class PlayerController final {
    public:
        PlayerController() = default;

        [[nodiscard]] bool Initialize(const CoreEngine::EngineContext &context) noexcept;

        void Update(const CoreEngine::FrameContext &frame) noexcept;

        void AttachCameraController(ThirdPersonCameraController &camera_controller) noexcept;

        void DetachCameraController() noexcept;

    private:
        [[nodiscard]] CoreEngine::NetworkPlayerInputState BuildNetworkInput(
            const CoreEngine::FrameContext &frame) const noexcept;

        [[nodiscard]] CoreEngine::Math::Vec2 BuildLookDeltaRadians(
            const CoreEngine::FrameContext &frame) const noexcept;

        ThirdPersonCameraController *camera_controller_ = nullptr;
        float mouse_sensitivity_x_ = 0.0025f;
        float mouse_sensitivity_y_ = 0.0025f;
        bool invert_y_ = false;
    };
} // Game

