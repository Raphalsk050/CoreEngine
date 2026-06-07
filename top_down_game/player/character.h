#pragma once
#include "../../core_engine/core/ability/i_gameplay_abiltity_interface.h"
#include "core/application/engine_context.h"
#include "core/ecs/node.h"
#include "i_possessable.h"

namespace TopDownGame {
    class Character : public IPossessable, public CoreEngine::IGameplayAbilityInterface {

    public:
        float GetHealth() override;
        float GetShield() override;
        float GetStamina() override;
        CoreEngine::Node &GetNode() override;
        float GetRunMovementSpeed() override;
        float GetWalkMovementSpeed() override;
        float GetJumpHeight() override;

        Character(const CoreEngine::EngineContext &context);
        void OnPossessed() override;
        void OnUnpossessed() override;
        void AddMovementInput(CoreEngine::InputVector2 input, float delta_time) override;

    protected:
        void InitializeCharacterRenderer();
        void InitializeCharacterAbilities();

    protected:
        bool is_possessed_ = false;
        CoreEngine::InputVector2 last_input_ = CoreEngine::InputVector2(0.0f, 0.0f);
        CoreEngine::InputVector2 current_input_ = CoreEngine::InputVector2(0.0f, 0.0f);
        CoreEngine::Node character_node_;
        const CoreEngine::EngineContext &context_;
    };

} // namespace TopDownGame
