#include "gameplay/systems/capture_system.h"

#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/math/math.h"
#include "core/network/network_system.h"
#include "core/network/replication/network_identity_component.h"
#include "core/network/replication/network_replicator.h"

namespace Game {
    namespace {
        constexpr float kCaptureRange = 2.5f;
        constexpr std::uint32_t kCaptureCooldownTicks = 90;

        [[nodiscard]] CoreEngine::NetworkEntityId NetworkIdForQueuedCommand(
            const CoreEngine::QueuedPlayerInputCommand &queued) noexcept {
            return queued.remote_user_id != 0u ? queued.remote_user_id : (0x10000000ull + queued.peer);
        }
    }

    bool CaptureSystem::CanStartCapture(const CoreEngine::HealthComponent &target) const noexcept {
        return target.alive && (target.concussed || target.health <= target.max_health * 0.25f);
    }

    void CaptureSystem::FixedUpdate(const GameplaySystemContext &context) {
        if (context.network_system.Session().Role() != CoreEngine::NetworkRole::Host) {
            return;
        }

        for (const CoreEngine::QueuedPlayerInputCommand &queued: context.network_system.InputCommands()) {
            if (!queued.command.IsButtonDown(CoreEngine::PlayerInputButton::Capture)) {
                continue;
            }

            std::uint32_t &next_allowed_tick = next_allowed_capture_tick_by_peer_[queued.peer];
            if (context.frame.tick < next_allowed_tick) {
                continue;
            }

            CoreEngine::Node captor = context.network_replicator.FindNode(NetworkIdForQueuedCommand(queued));
            if (!captor.IsValid()) {
                continue;
            }

            const auto *captor_transform = captor.TryGetComponent<CoreEngine::TransformComponent>();
            const auto *captor_identity = captor.TryGetComponent<CoreEngine::NetworkIdentityComponent>();
            if (captor_transform == nullptr || captor_identity == nullptr) {
                continue;
            }

            entt::entity best_target = entt::null;
            float best_distance_squared = kCaptureRange * kCaptureRange;
            auto view = context.world.View<CoreEngine::NetworkIdentityComponent,
                                           CoreEngine::TransformComponent,
                                           CoreEngine::HealthComponent>();
            for (const entt::entity entity: view) {
                const auto &target_identity = view.get<CoreEngine::NetworkIdentityComponent>(entity);
                const auto &target_health = view.get<CoreEngine::HealthComponent>(entity);
                if (target_identity.network_id == captor_identity->network_id || !CanStartCapture(target_health)) {
                    continue;
                }

                const auto &target_transform = view.get<CoreEngine::TransformComponent>(entity);
                const float distance_squared =
                    CoreEngine::Math::LengthSquared(target_transform.Position() - captor_transform->Position());
                if (distance_squared < best_distance_squared) {
                    best_distance_squared = distance_squared;
                    best_target = entity;
                }
            }

            if (best_target == entt::null) {
                next_allowed_tick = context.frame.tick + kCaptureCooldownTicks;
                continue;
            }

            auto &target_health = context.world.GetComponent<CoreEngine::HealthComponent>(best_target);
            target_health.concussed = true;

            auto *capture_state = context.world.TryGetComponent<CoreEngine::CaptureStateComponent>(best_target);
            if (capture_state == nullptr) {
                capture_state = &context.world.Emplace<CoreEngine::CaptureStateComponent>(best_target);
            }

            capture_state->captor_player = captor_identity->network_id;
            capture_state->cast_remaining_seconds = 0.0f;
            capture_state->capturable = true;
            capture_state->captured = true;

            next_allowed_tick = context.frame.tick + kCaptureCooldownTicks;
        }
    }
} // namespace Game
