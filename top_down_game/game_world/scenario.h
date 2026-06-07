#pragma once
#include "core/application/engine_context.h"

namespace TopDownGame {

    class Scenario {
    public:
        Scenario(const CoreEngine::EngineContext &context);
        void InitializeScenario();

    private:
        const CoreEngine::EngineContext &context_;
    };

} // namespace TopDownGame
