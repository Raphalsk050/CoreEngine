#pragma once

#include <span>
#include <vector>

#include "core/network/replication/replicated_state_types.h"
#include "gameplay_system_context.h"

namespace Game {
    /**
     * @brief Owns bounty beacon carrier/drop/extraction state.
     *
     * Responsibility: keep beacon ownership authoritative and queue high-priority
     * reliable state changes for replication.
     */
    class BountyBeaconSystem {
    public:
        void Reset();

        void RegisterBeacon(const CoreEngine::BountyBeaconComponent &beacon);

        void DropBeacon(CoreEngine::NetworkEntityId original_owner,
                        CoreEngine::NetworkEntityId carrier) noexcept;

        void FixedUpdate(const GameplaySystemContext &context) noexcept;

        [[nodiscard]] std::span<const CoreEngine::BountyBeaconComponent> Beacons() const noexcept {
            return beacons_;
        }

    private:
        std::vector<CoreEngine::BountyBeaconComponent> beacons_;
    };
} // namespace Game
