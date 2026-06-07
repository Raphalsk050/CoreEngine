#pragma once

namespace CoreEngine {
    class IGameplayAbilityInterface {
        virtual float GetHealth() = 0;
        virtual float GetShield() = 0;
        virtual float GetStamina() = 0;
        virtual float GetRunMovementSpeed() = 0;
        virtual float GetWalkMovementSpeed() = 0;
        virtual float GetJumpHeight() = 0;
    };
} // namespace CoreEngine
