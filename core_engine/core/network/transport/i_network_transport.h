#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "core/network/network_message.h"
#include "core/network/network_stats.h"

namespace CoreEngine {
    /**
     * @brief Transport-neutral network socket interface.
     *
     * Responsibility: let NetworkSystem route protocol messages without
     * gameplay or replication code depending on Steamworks transport details.
     */
    class INetworkTransport {
    public:
        virtual ~INetworkTransport() = default;

        virtual bool StartHost(int virtual_port, std::uint32_t max_peers) = 0;

        virtual bool ConnectToHost(std::uint64_t host_user_id, int virtual_port) = 0;

        virtual void Shutdown() = 0;

        virtual void PollEvents(NetworkEventQueue &out_events) = 0;

        virtual bool Send(PeerId peer, std::span<const std::byte> payload, SendMode mode) = 0;

        [[nodiscard]] virtual bool QueryMetrics(PeerId peer, NetworkConnectionMetrics &out_metrics) const = 0;

        [[nodiscard]] virtual std::string DetailedConnectionStatus(PeerId peer) const = 0;
    };
} // namespace CoreEngine
