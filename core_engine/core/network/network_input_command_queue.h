#pragma once

#include <cstdint>

#include "core/network/network_peer.h"
#include "core/network/prediction/player_input_command.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
    [[nodiscard]] constexpr NetworkEntityId MakeNetworkPlayerEntityId(PeerId peer,
                                                                       std::uint64_t user_id) noexcept {
        return user_id != 0u ? user_id : (0x10000000ull + peer);
    }

    /**
     * @brief Describes one player input command accepted by the network protocol.
     *
     * Responsibility: expose authoritative command streams to simulation systems
     * without requiring gameplay code to depend on the full network session owner.
     */
    struct QueuedPlayerInputCommand {
        PeerId peer = kInvalidPeerId;
        NetworkEntityId player_network_id = 0;
        std::uint64_t remote_user_id = 0;
        PlayerInputCommand command{};
    };
} // namespace CoreEngine
