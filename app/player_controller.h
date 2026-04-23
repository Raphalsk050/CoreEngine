#pragma once

#include "i_controller.h"
#include "player_command.h"

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

        [[nodiscard]] bool HasPossessable() const noexcept;

    private:
        [[nodiscard]] static PlayerCommand BuildCommand(const CoreEngine::FrameContext &frame) noexcept;

        IPossessable *possessable_ = nullptr;
    };
} // namespace Game