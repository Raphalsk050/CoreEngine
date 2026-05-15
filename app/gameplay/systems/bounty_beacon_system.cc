#include "gameplay/systems/bounty_beacon_system.h"

namespace Game {
    void BountyBeaconSystem::Reset() {
        beacons_.clear();
    }

    void BountyBeaconSystem::RegisterBeacon(const CoreEngine::BountyBeaconComponent &beacon) {
        beacons_.push_back(beacon);
    }

    void BountyBeaconSystem::DropBeacon(CoreEngine::NetworkEntityId original_owner,
                                        CoreEngine::NetworkEntityId carrier) noexcept {
        for (CoreEngine::BountyBeaconComponent &beacon: beacons_) {
            if (beacon.original_owner_player == original_owner && beacon.current_carrier_player == carrier) {
                beacon.current_carrier_player = 0;
                beacon.on_ground = true;
                beacon.extracted = false;
                return;
            }
        }
    }

    void BountyBeaconSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        (void) context;
    }
} // namespace Game
