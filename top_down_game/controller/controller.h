#pragma once

#pragma once

#include <memory>


#include "camera/camera.h"
#include "core/input/input_system.h"
#include "player/character.h"

namespace CoreEngine {
    struct EngineContext;
    struct FrameContext;
} // namespace CoreEngine

constexpr CoreEngine::InputActionId MoveAction = CoreEngine::MakeInputActionId(1);
constexpr CoreEngine::InputActionId LookAction = CoreEngine::MakeInputActionId(2);
constexpr CoreEngine::InputActionId SprintAction = CoreEngine::MakeInputActionId(3);

namespace TopDownGame {
    class Controller {

    public:
        explicit Controller(const CoreEngine::EngineContext &context);

        void Update(const CoreEngine::FrameContext &frame);

    private:
        bool BindControls() const;
        void UpdateCamera(const CoreEngine::FrameContext &frame) const;
        void Possess(IPossessable &possessable);
        void Unpossess();

    private:
        const CoreEngine::EngineContext &context_;
        CoreEngine::InputVector2 mouse_delta_;
        std::unique_ptr<Camera> camera_;
        IPossessable *possessed_ = nullptr;
    };

} // namespace TopDownGame
