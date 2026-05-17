#include "core/network/transport/steam_p2p_transport_adapter.h"

namespace CoreEngine {
    SteamP2PTransportAdapter::SteamP2PTransportAdapter(SteamOnlineSystem &online_system)
        : transport_(online_system) {
    }

    bool SteamP2PTransportAdapter::StartHost(int virtual_port, std::uint32_t max_peers) {
        return transport_.StartHost(static_cast<std::uint16_t>(virtual_port), max_peers);
    }

    bool SteamP2PTransportAdapter::ConnectToHost(std::uint64_t host_user_id, int virtual_port) {
        return transport_.ConnectToHost(host_user_id, static_cast<std::uint16_t>(virtual_port));
    }

    void SteamP2PTransportAdapter::Shutdown() {
        transport_.Shutdown();
    }

    void SteamP2PTransportAdapter::PollEvents(NetworkEventQueue &out_events) {
        transport_.PollEvents(out_events);
    }

    bool SteamP2PTransportAdapter::Send(PeerId peer, std::span<const std::byte> payload, SendMode mode) {
        return transport_.Send(peer, payload, mode);
    }

    bool SteamP2PTransportAdapter::QueryMetrics(PeerId peer, NetworkConnectionMetrics &out_metrics) const {
        return transport_.QueryMetrics(peer, out_metrics);
    }

    std::string SteamP2PTransportAdapter::DetailedConnectionStatus(PeerId peer) const {
        return transport_.DetailedConnectionStatus(peer);
    }
} // namespace CoreEngine
