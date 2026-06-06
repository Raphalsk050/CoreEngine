#pragma once
#include "character.h"
#include "core/application/frame_context.h"
#include "core/assert/assert.h"
#include "core/ecs/node.h"

namespace TopDownGame {

    class Player final : public Character {
    public:
        Player(const CoreEngine::EngineContext &context);
        void Init();
        void Update(const CoreEngine::FrameContext &frame);
    };

} // namespace TopDownGame
