#pragma once

namespace TopDownGame {
    class IGameplayAbilityInterface {
        virtual float GetMovementSpeed() = 0;
        virtual float GetJumpHeight() = 0;
        virtual float GetStamina() = 0;
        virtual float GetHealth() = 0;
        virtual float GetShield() = 0;
    };
} // namespace TopDownGame
