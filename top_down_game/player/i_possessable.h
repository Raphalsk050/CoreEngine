#pragma once
#include "core/input/input_system.h"

namespace TopDownGame {
    class IPossessable {
    public:
        virtual ~IPossessable() = default;
        virtual void OnPossessed() = 0;
        virtual void OnUnpossessed() = 0;
        virtual void AddMovementInput(CoreEngine::InputVector2 input) = 0;
    };
} // namespace TopDownGame
