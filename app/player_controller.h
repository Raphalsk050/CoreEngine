#pragma once

#include "i_controller.h"
#include "player_command.h"
#include "third_person_camera_controller.h"

namespace CoreEngine {
    struct EngineContext;
}

namespace Game {
    class PlayerController final : public IController {
    public:
        PlayerController() = default;

        [[nodiscard]] bool Init(const CoreEngine::EngineContext &context);

        void Update(const CoreEngine::FrameContext &frame) override;

        void Possess(IPossessable &possessable) override;

        void Unpossess() override;

        void AttachCameraController(ThirdPersonCameraController &camera_controller) noexcept;

        void DetachCameraController() noexcept;

        [[nodiscard]] bool HasPossessable() const noexcept;

    private:
        [[nodiscard]] PlayerCommand BuildCommand(const CoreEngine::FrameContext &frame) const noexcept;

        [[nodiscard]] CoreEngine::Math::Vec2 BuildLookDelta(const CoreEngine::FrameContext &frame) const noexcept;

        IPossessable *possessable_ = nullptr;
        ThirdPersonCameraController *camera_controller_ = nullptr;

        float mouse_sensitivity_x_ = 0.12f;
        float mouse_sensitivity_y_ = 0.12f;
        bool invert_y_ = false;
    };
} // namespace Game
