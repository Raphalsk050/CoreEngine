#pragma once
#include "i_controller.h"
#include "movement_type.h"


namespace CoreEngine {
    struct FrameContext;
    struct EngineContext;
}

namespace Game {
    class PlayerController final : public IController {
    public:
        explicit PlayerController(IPossessable &possessable, MovementType movement_type = MovementType::WALK)
            : movement_type_(movement_type),
              possessable_(possessable) {
        }

        void Init(const CoreEngine::EngineContext &context);

        void ProcessEvents(const CoreEngine::FrameContext &frame);

        void Possess(IPossessable &possessable) override;

        void Unpossess() override;

    private:
        MovementType movement_type_{MovementType::WALK};
        IPossessable &possessable_;
    };
}
