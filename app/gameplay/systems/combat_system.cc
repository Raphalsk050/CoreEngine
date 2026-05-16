#include "gameplay/systems/combat_system.h"

#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/math/math.h"
#include "core/network/network_system.h"
#include "core/network/replication/network_identity_component.h"
#include "core/network/replication/network_replicator.h"

#include <algorithm>

namespace Game {
    namespace {
        constexpr float kHitscanRange = 35.0f;
        constexpr float kBaseDamage = 34.0f;
        constexpr std::uint32_t kFireCooldownTicks = 12;

        [[nodiscard]] CoreEngine::ArmorPart &SelectArmorPart(CoreEngine::ArmorSegmentsComponent &armor,
                                                             HitRegion region) noexcept {
            switch (region) {
                case HitRegion::Head:
                    return armor.head;
                case HitRegion::Torso:
                    return armor.torso;
                case HitRegion::LeftArm:
                    return armor.left_arm;
                case HitRegion::RightArm:
                    return armor.right_arm;
                case HitRegion::Legs:
                    return armor.legs;
            }

            return armor.torso;
        }

        [[nodiscard]] CoreEngine::NetworkEntityId NetworkIdForQueuedCommand(
            const CoreEngine::QueuedPlayerInputCommand &queued) noexcept {
            return queued.remote_user_id != 0u ? queued.remote_user_id : (0x10000000ull + queued.peer);
        }
    }

    float CombatSystem::ComputeDamage(float base_damage, HitRegion region) const noexcept {
        return region == HitRegion::Head ? base_damage * 1.5f : base_damage;
    }

    void CombatSystem::FixedUpdate(const GameplaySystemContext &context) {
        if (context.network_system.Session().Role() != CoreEngine::NetworkRole::Host) {
            return;
        }

        for (const CoreEngine::QueuedPlayerInputCommand &queued: context.network_system.InputCommands()) {
            if (!queued.command.IsButtonDown(CoreEngine::PlayerInputButton::Fire)) {
                continue;
            }

            std::uint32_t &next_allowed_tick = next_allowed_fire_tick_by_peer_[queued.peer];
            if (context.frame.tick < next_allowed_tick) {
                continue;
            }

            CoreEngine::Node attacker = context.network_replicator.FindNode(NetworkIdForQueuedCommand(queued));
            if (!attacker.IsValid()) {
                continue;
            }

            const auto *attacker_transform = attacker.TryGetComponent<CoreEngine::TransformComponent>();
            const auto *attacker_identity = attacker.TryGetComponent<CoreEngine::NetworkIdentityComponent>();
            if (attacker_transform == nullptr || attacker_identity == nullptr) {
                continue;
            }

            const CoreEngine::Math::Vec3 attacker_position = attacker_transform->Position();
            float best_distance_squared = kHitscanRange * kHitscanRange;
            entt::entity best_target = entt::null;

            auto view = context.world.View<CoreEngine::NetworkIdentityComponent,
                                           CoreEngine::TransformComponent,
                                           CoreEngine::HealthComponent>();
            for (const entt::entity entity: view) {
                const auto &target_identity = view.get<CoreEngine::NetworkIdentityComponent>(entity);
                const auto &target_health = view.get<CoreEngine::HealthComponent>(entity);
                if (!target_identity.IsNetworked() ||
                    target_identity.network_id == attacker_identity->network_id ||
                    !target_health.alive) {
                    continue;
                }

                const auto &target_transform = view.get<CoreEngine::TransformComponent>(entity);
                const float distance_squared =
                    CoreEngine::Math::LengthSquared(target_transform.Position() - attacker_position);
                if (distance_squared < best_distance_squared) {
                    best_distance_squared = distance_squared;
                    best_target = entity;
                }
            }

            if (best_target == entt::null) {
                next_allowed_tick = context.frame.tick + kFireCooldownTicks;
                continue;
            }

            auto &health = context.world.GetComponent<CoreEngine::HealthComponent>(best_target);
            float damage = ComputeDamage(kBaseDamage, HitRegion::Torso);
            if (auto *armor = context.world.TryGetComponent<CoreEngine::ArmorSegmentsComponent>(best_target);
                armor != nullptr) {
                CoreEngine::ArmorPart &part = SelectArmorPart(*armor, HitRegion::Torso);
                const float absorbed = std::min(part.hit_points, damage);
                part.hit_points -= absorbed;
                damage -= absorbed;
            }

            health.health = std::max(0.0f, health.health - damage);
            if (health.health <= health.max_health * 0.25f && health.health > 0.0f) {
                health.concussed = true;
            }

            next_allowed_tick = context.frame.tick + kFireCooldownTicks;
        }
    }
} // namespace Game
