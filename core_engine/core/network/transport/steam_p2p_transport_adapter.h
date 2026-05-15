#pragma once

#include "core/network/transport/i_network_transport.h"
#include "core/online/steam/steam_p2p_transport.h"

namespace CoreEngine {
    class SteamOnlineSystem;

    /**
     * @brief Adapts Steam Networking Sockets to INetworkTransport.
     *
     * Responsibility: isolate Steam P2P socket calls behind the engine transport
     * abstraction used by NetworkSystem.
     */
    class SteamP2PTransportAdapter final : public INetworkTransport {
    public:
        explicit SteamP2PTransportAdapter(SteamOnlineSystem &online_system);

        bool StartHost(int virtual_port, std::uint32_t max_peers) override;

        bool ConnectToHost(std::uint64_t host_user_id, int virtual_port) override;

        void Shutdown() override;

        void PollEvents(NetworkEventQueue &out_events) override;

        bool Send(PeerId peer, std::span<const std::byte> payload, SendMode mode) override;

        [[nodiscard]] std::string DetailedConnectionStatus(PeerId peer) const override;

    private:
        SteamP2PTransport transport_;
    };
} // namespace CoreEngine
