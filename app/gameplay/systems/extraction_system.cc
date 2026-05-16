#include "gameplay/systems/extraction_system.h"

#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/math/math.h"
#include "core/network/network_system.h"
#include "core/network/replication/network_identity_component.h"
#include "core/network/replication/network_replicator.h"

namespace Game {
    namespace {
        constexpr float kExtractionRadius = 4.0f;
        constexpr float kShipArrivalSeconds = 8.0f;
        constexpr float kBoardingSeconds = 8.0f;
        constexpr float kLzY = 0.0f;

        [[nodiscard]] CoreEngine::Math::Vec3 LzPosition() noexcept {
            return {0.0f, kLzY, -2.0f};
        }

        [[nodiscard]] CoreEngine::NetworkEntityId NetworkIdForQueuedCommand(
            const CoreEngine::QueuedPlayerInputCommand &queued) noexcept {
            return queued.remote_user_id != 0u ? queued.remote_user_id : (0x10000000ull + queued.peer);
        }
    }

    void ExtractionSystem::Activate(CoreEngine::ExtractionStateComponent &state,
                                    float arrival_seconds) const noexcept {
        state.state = CoreEngine::ExtractionState::ShipInbound;
        state.timer_seconds = arrival_seconds;
        state.public_event_active = true;
    }

    void ExtractionSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        if (context.network_system.Session().Role() != CoreEngine::NetworkRole::Host) {
            return;
        }

        if (state_.state == CoreEngine::ExtractionState::ShipInbound ||
            state_.state == CoreEngine::ExtractionState::BoardingOpen) {
            state_.timer_seconds -= context.frame.fixed_delta_time;
            if (state_.timer_seconds <= 0.0f) {
                if (state_.state == CoreEngine::ExtractionState::ShipInbound) {
                    state_.state = CoreEngine::ExtractionState::BoardingOpen;
                    state_.timer_seconds = kBoardingSeconds;
                } else {
                    state_.state = CoreEngine::ExtractionState::Departed;
                    state_.timer_seconds = 0.0f;
                    state_.public_event_active = false;
                }
            }
        }

        for (const CoreEngine::QueuedPlayerInputCommand &queued: context.network_system.InputCommands()) {
            if (!queued.command.IsButtonDown(CoreEngine::PlayerInputButton::Interact)) {
                continue;
            }

            CoreEngine::Node player = context.network_replicator.FindNode(NetworkIdForQueuedCommand(queued));
            if (!player.IsValid()) {
                continue;
            }

            const auto *transform = player.TryGetComponent<CoreEngine::TransformComponent>();
            const auto *identity = player.TryGetComponent<CoreEngine::NetworkIdentityComponent>();
            if (transform == nullptr || identity == nullptr) {
                continue;
            }

            const float distance_squared = CoreEngine::Math::LengthSquared(transform->Position() - LzPosition());
            if (distance_squared > kExtractionRadius * kExtractionRadius) {
                continue;
            }

            if (state_.state == CoreEngine::ExtractionState::Closed ||
                state_.state == CoreEngine::ExtractionState::Departed) {
                Activate(state_, kShipArrivalSeconds);
                continue;
            }

            if (state_.state != CoreEngine::ExtractionState::BoardingOpen) {
                continue;
            }

            auto *carrier = player.TryGetComponent<CoreEngine::BountyBeaconCarrierComponent>();
            if (carrier == nullptr) {
                continue;
            }

            auto beacon_view = context.world.View<CoreEngine::BountyBeaconComponent>();
            for (const entt::entity entity: beacon_view) {
                auto &beacon = beacon_view.get<CoreEngine::BountyBeaconComponent>(entity);
                if (beacon.current_carrier_player == identity->network_id) {
                    beacon.extracted = true;
                    beacon.on_ground = false;
                }
            }
        }
    }
} // namespace Game
