#include "character.h"

#include <cstddef>

#include "core/ability/ability_component.h"
#include "core/ability/ability_system.h"
#include "core/ecs/world.h"
#include "core/render/render_system.h"

namespace TopDownGame {
    namespace {
        [[nodiscard]] float ResolveAttributeValue(CoreEngine::AbilitySystem &ability_system, CoreEngine::Node node,
                                                  std::size_t attribute_index,
                                                  const CoreEngine::Attribute &fallback_attribute) {
            CoreEngine::AbilityComponent *ability_component = ability_system.EnsureAbilityComponent(node);
            if (ability_component == nullptr) {
                return fallback_attribute.current_value;
            }

            return ability_component->attributes[attribute_index].current_value;
        }
    } // namespace

    Character::Character(const CoreEngine::EngineContext &context) : context_(context) {

        character_node_ = context_.world.CreateNode("Character");

        InitializeCharacterAbilities();
        InitializeCharacterRenderer();
    }

    void Character::OnPossessed() {
        is_possessed_ = true;
    }

    void Character::OnUnpossessed() {
        is_possessed_ = false;
    }

    void Character::AddMovementInput(CoreEngine::InputVector2 input, float delta_time) {
        last_input_ = current_input_;
        current_input_ = input;

        // TODO(rafael): In the future, this direct position-setting approach should probably be handled by the
        // physics system. Something like AddVelocity() or a similar method would be preferable.
        // This would be better than the current approach. This would be better than this shitty way.
        if (character_node_.IsValid()) {
            character_node_.SetPosition(
                    character_node_.GetPosition() +
                    CoreEngine::Math::Normalize(CoreEngine::Math::Vec3(current_input_.x, 0.0f, current_input_.y)) *
                            GetWalkMovementSpeed() * delta_time);
        }
    }

    float Character::GetWalkMovementSpeed() {
        return ResolveAttributeValue(context_.ability_system, character_node_, CoreEngine::WalkSpeedAttributeIndex,
                                     CoreEngine::BaseAttributes::walk_speed_attribute);
    }

    float Character::GetRunMovementSpeed() {
        return ResolveAttributeValue(context_.ability_system, character_node_, CoreEngine::RunSpeedAttributeIndex,
                                     CoreEngine::BaseAttributes::run_speed_attribute);
    }

    float Character::GetJumpHeight() {
        return ResolveAttributeValue(context_.ability_system, character_node_, CoreEngine::JumpHeightAttributeIndex,
                                     CoreEngine::BaseAttributes::jump_height_attribute);
    }

    float Character::GetStamina() {
        return ResolveAttributeValue(context_.ability_system, character_node_, CoreEngine::StaminaAttributeIndex,
                                     CoreEngine::BaseAttributes::stamina_attribute);
    }

    CoreEngine::Node &Character::GetNode() {
        return character_node_;
    }

    float Character::GetHealth() {
        return ResolveAttributeValue(context_.ability_system, character_node_, CoreEngine::LifeAttributeIndex,
                                     CoreEngine::BaseAttributes::life_attribute);
    }

    float Character::GetShield() {
        return ResolveAttributeValue(context_.ability_system, character_node_, CoreEngine::ShieldAttributeIndex,
                                     CoreEngine::BaseAttributes::shield_attribute);
    }

    void Character::InitializeCharacterRenderer() {
        (void) context_.render_system.SetPrimitiveRenderer(
                character_node_, CoreEngine::PrimitiveRendererDesc::Unlit(CoreEngine::PrimitiveType::Sphere,
                                                                          CoreEngine::Math::Vec4(1.0f)));
    }

    void Character::InitializeCharacterAbilities() {
        (void) context_.ability_system.EnsureAbilityComponent(character_node_);
    }
} // namespace TopDownGame
