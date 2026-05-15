#pragma once

#include <cstdint>

#include "core/network/network_peer.h"

namespace CoreEngine {
    using NetworkEntityId = std::uint64_t;

    /**
     * @brief Identifies an entity replicated over the network.
     *
     * Responsibility: separate stable network identity and ownership from local
     * ECS storage so snapshots do not depend on transient entity handles.
     */
    struct NetworkIdentityComponent {
        NetworkEntityId network_id = 0;
        PeerId owner_peer = kInvalidPeerId;
        bool local_authority = false;
        bool replicated = true;

        [[nodiscard]] bool IsNetworked() const noexcept {
            return replicated && network_id != 0;
        }
    };
} // namespace CoreEngine
