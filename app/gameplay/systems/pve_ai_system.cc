#include "gameplay/systems/pve_ai_system.h"

#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/math/math.h"
#include "core/network/network_system.h"
#include "core/network/replication/network_replicator.h"
#include "core/network/replication/replicated_state_types.h"

#include <algorithm>

namespace Game {
    namespace {
        constexpr CoreEngine::NetworkEntityId kDefaultAiNetworkId = 0xA1000001ull;
        constexpr float kAiMoveSpeed = 2.0f;
        constexpr float kAiAggroRange = 40.0f;
        constexpr float kAiAttackRange = 1.6f;
        constexpr float kAiDamage = 8.0f;
        constexpr std::uint32_t kAiAttackCooldownTicks = 45;
    }

    void PvEAISystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        if (context.network_system.Session().Role() == CoreEngine::NetworkRole::Client) {
            return;
        }

        if (ai_id_ == 0) {
            CoreEngine::Node ai = context.world.CreateNode("PvEHunter");
            ai.SetPosition({0.0f, 0.0f, 6.0f});
            ai.SetScale({0.75f, 1.7f, 0.75f});
            ai_id_ = context.network_replicator.RegisterEntity(
                ai,
                kDefaultAiNetworkId,
                CoreEngine::kInvalidPeerId,
                true);
            ai.AddComponent<CoreEngine::AIStateComponent>();
            ai.AddComponent<CoreEngine::HealthComponent>(CoreEngine::HealthComponent{
                .health = 150.0f,
                .max_health = 150.0f,
                .alive = true,
                .concussed = false,
            });
        }

        CoreEngine::Node ai = context.network_replicator.FindNode(ai_id_);
        if (!ai.IsValid()) {
            ai_id_ = 0;
            return;
        }

        auto *ai_transform = ai.TryGetComponent<CoreEngine::TransformComponent>();
        auto *ai_state = ai.TryGetComponent<CoreEngine::AIStateComponent>();
        auto *ai_health = ai.TryGetComponent<CoreEngine::HealthComponent>();
        if (ai_transform == nullptr || ai_state == nullptr || ai_health == nullptr || !ai_health->alive) {
            if (ai_state != nullptr) {
                ai_state->state = CoreEngine::AIState::Dead;
            }
            return;
        }

        entt::entity best_target = entt::null;
        float best_distance_squared = kAiAggroRange * kAiAggroRange;
        auto view = context.world.View<CoreEngine::NetworkIdentityComponent,
                                       CoreEngine::TransformComponent,
                                       CoreEngine::HealthComponent>();
        for (const entt::entity entity: view) {
            const auto &identity = view.get<CoreEngine::NetworkIdentityComponent>(entity);
            const auto &health = view.get<CoreEngine::HealthComponent>(entity);
            if (identity.network_id == ai_id_ || !health.alive) {
                continue;
            }

            const auto &transform = view.get<CoreEngine::TransformComponent>(entity);
            const float distance_squared =
                CoreEngine::Math::LengthSquared(transform.Position() - ai_transform->Position());
            if (distance_squared < best_distance_squared) {
                best_distance_squared = distance_squared;
                best_target = entity;
            }
        }

        if (best_target == entt::null) {
            ai_state->state = CoreEngine::AIState::Patrol;
            ai_state->target_entity = 0;
            return;
        }

        auto &target_identity = context.world.GetComponent<CoreEngine::NetworkIdentityComponent>(best_target);
        auto &target_transform = context.world.GetComponent<CoreEngine::TransformComponent>(best_target);
        auto &target_health = context.world.GetComponent<CoreEngine::HealthComponent>(best_target);
        ai_state->target_entity = target_identity.network_id;

        const CoreEngine::Math::Vec3 to_target = target_transform.Position() - ai_transform->Position();
        const float distance_squared = CoreEngine::Math::LengthSquared(to_target);
        if (distance_squared <= kAiAttackRange * kAiAttackRange) {
            ai_state->state = CoreEngine::AIState::Attack;
            if (context.frame.tick >= next_attack_tick_) {
                target_health.health = std::max(0.0f, target_health.health - kAiDamage);
                next_attack_tick_ = context.frame.tick + kAiAttackCooldownTicks;
            }
            return;
        }

        ai_state->state = CoreEngine::AIState::Chase;
        if (distance_squared > 0.0001f) {
            const CoreEngine::Math::Vec3 direction = CoreEngine::Math::Normalize(to_target);
            ai_transform->SetPosition(
                ai_transform->Position() + direction * kAiMoveSpeed * context.frame.fixed_delta_time);
        }
    }
} // namespace Game
