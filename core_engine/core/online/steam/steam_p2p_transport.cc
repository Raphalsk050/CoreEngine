#include "core/online/steam/steam_p2p_transport.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "core/log/log.h"
#include "core/online/steam/steam_online_system.h"

#if CORE_ENGINE_ENABLE_STEAM
#include "steam/isteamnetworkingutils.h"
#endif

namespace CoreEngine {
#if CORE_ENGINE_ENABLE_STEAM
    namespace {
        [[nodiscard]] std::array<SteamNetworkingConfigValue_t, 2> BuildP2PConfig() noexcept {
            std::array<SteamNetworkingConfigValue_t, 2> options{};
            options[0].SetInt32(k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable,
                                k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_All);
            options[1].SetInt32(k_ESteamNetworkingConfig_P2P_Transport_SDR_Penalty, 250);
            return options;
        }

        [[nodiscard]] int ToSteamSendFlags(SendMode mode) noexcept {
            switch (mode) {
                case SendMode::Unreliable:
                    return k_nSteamNetworkingSend_Unreliable;
                case SendMode::UnreliableNoDelay:
                    return k_nSteamNetworkingSend_UnreliableNoDelay;
                case SendMode::Reliable:
                    return k_nSteamNetworkingSend_Reliable;
                case SendMode::ReliableNoNagle:
                    return k_nSteamNetworkingSend_Reliable | k_nSteamNetworkingSend_NoNagle;
            }

            return k_nSteamNetworkingSend_Unreliable;
        }

        [[nodiscard]] const char *AvailabilityName(ESteamNetworkingAvailability availability) noexcept {
            switch (availability) {
                case k_ESteamNetworkingAvailability_CannotTry:
                    return "CannotTry";
                case k_ESteamNetworkingAvailability_Failed:
                    return "Failed";
                case k_ESteamNetworkingAvailability_Previously:
                    return "Previously";
                case k_ESteamNetworkingAvailability_Retrying:
                    return "Retrying";
                case k_ESteamNetworkingAvailability_NeverTried:
                    return "NeverTried";
                case k_ESteamNetworkingAvailability_Waiting:
                    return "Waiting";
                case k_ESteamNetworkingAvailability_Attempting:
                    return "Attempting";
                case k_ESteamNetworkingAvailability_Current:
                    return "Current";
                case k_ESteamNetworkingAvailability_Unknown:
                    return "Unknown";
                default:
                    return "Other";
            }
        }

        [[nodiscard]] int MicrosecondsToMilliseconds(SteamNetworkingMicroseconds value) noexcept {
            return value > 0 ? static_cast<int>((value + 999) / 1000) : 0;
        }

        [[nodiscard]] std::uint32_t PositiveCounter(int value) noexcept {
            return value > 0 ? static_cast<std::uint32_t>(value) : 0u;
        }

        void AppendPop(std::string &output, const char *label, SteamNetworkingPOPID pop) {
            if (pop == 0) {
                return;
            }

            output.append(label);
            output.append(SteamNetworkingPOPIDRender(pop).c_str());
            output.append("\n");
        }

        void AppendConnectionInfo(std::string &output, HSteamNetConnection connection) {
            SteamNetConnectionInfo_t info{};
            if (!SteamNetworkingSockets()->GetConnectionInfo(connection, &info)) {
                return;
            }

            output.append("\nConnection info:\n");
            output.append("  Route flags:");
            output.append((info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Relayed) != 0 ? " relayed" : " direct-or-unknown");
            if ((info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Fast) != 0) {
                output.append(" fast-lan");
            }
            if ((info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Unauthenticated) != 0) {
                output.append(" unauthenticated");
            }
            if ((info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_DualWifi) != 0) {
                output.append(" dual-wifi");
            }
            output.append("\n");
            AppendPop(output, "  Remote POP: ", info.m_idPOPRemote);
            AppendPop(output, "  Relay POP: ", info.m_idPOPRelay);
            if (!info.m_addrRemote.IsIPv6AllZeros()) {
                output.append("  Remote address: ");
                output.append(SteamNetworkingIPAddrRender(info.m_addrRemote, true).c_str());
                output.append("\n");
            }
        }

        void AppendRealtimeQueueStatus(std::string &output, HSteamNetConnection connection) {
            SteamNetConnectionRealTimeStatus_t status{};
            if (SteamNetworkingSockets()->GetConnectionRealTimeStatus(connection, &status, 0, nullptr) != k_EResultOK) {
                return;
            }

            output.append("\nRealtime queue:\n");
            output.append("  Queue time: ");
            output.append(std::to_string(MicrosecondsToMilliseconds(status.m_usecQueueTime)));
            output.append(" ms\n  Pending unreliable/reliable: ");
            output.append(std::to_string(PositiveCounter(status.m_cbPendingUnreliable)));
            output.append(" / ");
            output.append(std::to_string(PositiveCounter(status.m_cbPendingReliable)));
            output.append(" bytes\n  Send rate limit: ");
            output.append(std::to_string(PositiveCounter(status.m_nSendRateBytesPerSecond)));
            output.append(" B/s\n");
        }

        void AppendRelayDiagnostics(std::string &output) {
            if (SteamNetworkingUtils() == nullptr) {
                return;
            }

            SteamRelayNetworkStatus_t relay_status{};
            const ESteamNetworkingAvailability availability =
                SteamNetworkingUtils()->GetRelayNetworkStatus(&relay_status);
            output.append("\nRelay network:\n  Availability: ");
            output.append(AvailabilityName(availability));
            output.append("\n  Ping measurement: ");
            output.append(relay_status.m_bPingMeasurementInProgress != 0 ? "in progress" : "idle");
            if (relay_status.m_debugMsg[0] != '\0') {
                output.append("\n  Status: ");
                output.append(relay_status.m_debugMsg);
            }

            const SteamNetworkingPOPID kGruPop = CalculateSteamNetworkingPOPIDFromString("gru");
            SteamNetworkingPOPID via_pop = 0;
            const int routed_ping = SteamNetworkingUtils()->GetPingToDataCenter(kGruPop, &via_pop);
            const int direct_ping = SteamNetworkingUtils()->GetDirectPingToPOP(kGruPop);
            output.append("\n  GRU relay ping direct/routed: ");
            output.append(std::to_string(direct_ping));
            output.append(" / ");
            output.append(std::to_string(routed_ping));
            output.append(" ms");
            if (via_pop != 0 && via_pop != kGruPop) {
                output.append(" via ");
                output.append(SteamNetworkingPOPIDRender(via_pop).c_str());
            }
            output.append("\n");
        }
    }
#endif

    SteamP2PTransport::SteamP2PTransport(SteamOnlineSystem &online_system)
#if CORE_ENGINE_ENABLE_STEAM
        : connection_status_callback_(this, &SteamP2PTransport::OnConnectionStatusChanged),
          online_system_(online_system) {
    }
#else
        : online_system_(online_system) {
    }
#endif

    SteamP2PTransport::~SteamP2PTransport() {
        Shutdown();
    }

    bool SteamP2PTransport::StartHost(std::uint16_t virtual_port, std::uint32_t max_peers) {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamNetworkingSockets() == nullptr || max_peers == 0) {
            return false;
        }

        Shutdown();
        poll_group_ = SteamNetworkingSockets()->CreatePollGroup();
        if (poll_group_ == k_HSteamNetPollGroup_Invalid) {
            return false;
        }

        const auto p2p_options = BuildP2PConfig();
        listen_socket_ = SteamNetworkingSockets()->CreateListenSocketP2P(
            virtual_port,
            static_cast<int>(p2p_options.size()),
            p2p_options.data());
        if (listen_socket_ == k_HSteamListenSocket_Invalid) {
            SteamNetworkingSockets()->DestroyPollGroup(poll_group_);
            poll_group_ = k_HSteamNetPollGroup_Invalid;
            return false;
        }

        is_host_ = true;
        max_peers_ = max_peers;
        next_peer_id_ = kHostPeerId;
        return true;
#else
        (void) virtual_port;
        (void) max_peers;
        return false;
#endif
    }

    bool SteamP2PTransport::ConnectToHost(std::uint64_t host_steam_id, std::uint16_t virtual_port) {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamNetworkingSockets() == nullptr || host_steam_id == 0) {
            return false;
        }

        Shutdown();
        poll_group_ = SteamNetworkingSockets()->CreatePollGroup();
        if (poll_group_ == k_HSteamNetPollGroup_Invalid) {
            return false;
        }

        SteamNetworkingIdentity identity;
        identity.Clear();
        identity.SetSteamID64(host_steam_id);

        const auto p2p_options = BuildP2PConfig();
        const HSteamNetConnection connection = SteamNetworkingSockets()->ConnectP2P(
            identity,
            virtual_port,
            static_cast<int>(p2p_options.size()),
            p2p_options.data());
        if (connection == k_HSteamNetConnection_Invalid) {
            SteamNetworkingSockets()->DestroyPollGroup(poll_group_);
            poll_group_ = k_HSteamNetPollGroup_Invalid;
            return false;
        }

        SteamNetworkingSockets()->SetConnectionPollGroup(connection, poll_group_);
        is_host_ = false;
        max_peers_ = 1;
        RegisterConnection(host_steam_id, connection, NetworkPeerState::Connecting);
        return true;
#else
        (void) host_steam_id;
        (void) virtual_port;
        return false;
#endif
    }

    void SteamP2PTransport::Shutdown() {
#if CORE_ENGINE_ENABLE_STEAM
        if (SteamNetworkingSockets() != nullptr) {
            for (const auto &[connection, peer]: conn_to_peer_) {
                (void) peer;
                SteamNetworkingSockets()->CloseConnection(connection, 0, "Transport shutdown", false);
            }

            if (listen_socket_ != k_HSteamListenSocket_Invalid) {
                SteamNetworkingSockets()->CloseListenSocket(listen_socket_);
            }

            if (poll_group_ != k_HSteamNetPollGroup_Invalid) {
                SteamNetworkingSockets()->DestroyPollGroup(poll_group_);
            }
        }

        listen_socket_ = k_HSteamListenSocket_Invalid;
        poll_group_ = k_HSteamNetPollGroup_Invalid;
        conn_to_peer_.clear();
        peer_to_conn_.clear();
        conn_to_steam_id_.clear();
#endif

        pending_events_.clear();
        is_host_ = false;
        max_peers_ = 0;
        next_peer_id_ = kHostPeerId;
    }

    void SteamP2PTransport::PollEvents(NetworkEventQueue &out_events) {
        out_events.insert(out_events.end(),
                          std::make_move_iterator(pending_events_.begin()),
                          std::make_move_iterator(pending_events_.end()));
        pending_events_.clear();

#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamNetworkingSockets() == nullptr ||
            poll_group_ == k_HSteamNetPollGroup_Invalid) {
            return;
        }

        std::array<SteamNetworkingMessage_t *, 64> messages{};
        int count = 0;
        do {
            count = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(
                poll_group_,
                messages.data(),
                static_cast<int>(messages.size()));

            for (int i = 0; i < count; ++i) {
                SteamNetworkingMessage_t *message = messages[static_cast<std::size_t>(i)];
                if (message == nullptr) {
                    continue;
                }

                const auto peer_it = conn_to_peer_.find(message->m_conn);
                NetworkEvent event;
                event.type = NetworkEventType::PacketReceived;
                event.peer = peer_it != conn_to_peer_.end() ? peer_it->second : kInvalidPeerId;
                event.remote_steam_id = conn_to_steam_id_.contains(message->m_conn) ? conn_to_steam_id_[message->m_conn] : 0;

                if (message->m_cbSize > 0 && message->m_pData != nullptr) {
                    event.payload.resize(static_cast<std::size_t>(message->m_cbSize));
                    std::memcpy(event.payload.data(), message->m_pData, event.payload.size());
                }

                out_events.push_back(std::move(event));
                message->Release();
            }
        } while (count == static_cast<int>(messages.size()));
#endif
    }

    bool SteamP2PTransport::Send(PeerId peer, std::span<const std::byte> payload, SendMode mode) {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamNetworkingSockets() == nullptr || payload.empty()) {
            return false;
        }

        const auto it = peer_to_conn_.find(peer);
        if (it == peer_to_conn_.end()) {
            return false;
        }

        const EResult result = SteamNetworkingSockets()->SendMessageToConnection(
            it->second,
            payload.data(),
            static_cast<std::uint32_t>(payload.size()),
            ToSteamSendFlags(mode),
            nullptr);
        return result == k_EResultOK;
#else
        (void) peer;
        (void) payload;
        (void) mode;
        return false;
#endif
    }

    bool SteamP2PTransport::QueryMetrics(PeerId peer, NetworkConnectionMetrics &out_metrics) const {
        out_metrics = {};
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamNetworkingSockets() == nullptr) {
            return false;
        }

        const auto it = peer_to_conn_.find(peer);
        if (it == peer_to_conn_.end()) {
            return false;
        }

        SteamNetConnectionRealTimeStatus_t status{};
        const EResult result =
            SteamNetworkingSockets()->GetConnectionRealTimeStatus(it->second, &status, 0, nullptr);
        if (result != k_EResultOK) {
            return false;
        }

        out_metrics.ping_ms = status.m_nPing >= 0 ? status.m_nPing : -1;
        out_metrics.jitter_ms = status.m_usecMaxJitter > 0 ? static_cast<int>(status.m_usecMaxJitter / 1000) : 0;
        out_metrics.queue_time_ms = MicrosecondsToMilliseconds(status.m_usecQueueTime);
        out_metrics.pending_unreliable_bytes = PositiveCounter(status.m_cbPendingUnreliable);
        out_metrics.pending_reliable_bytes = PositiveCounter(status.m_cbPendingReliable);
        out_metrics.send_rate_bytes_per_second = PositiveCounter(status.m_nSendRateBytesPerSecond);
        if (status.m_flConnectionQualityLocal >= 0.0f) {
            out_metrics.packet_loss = std::clamp(1.0f - status.m_flConnectionQualityLocal, 0.0f, 1.0f);
        }
        out_metrics.valid = out_metrics.ping_ms >= 0;
        return out_metrics.valid;
#else
        (void) peer;
        return false;
#endif
    }

    std::string SteamP2PTransport::DetailedConnectionStatus(PeerId peer) const {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamNetworkingSockets() == nullptr) {
            return {};
        }

        const auto it = peer_to_conn_.find(peer);
        if (it == peer_to_conn_.end()) {
            return {};
        }

        char status[4096]{};
        SteamNetworkingSockets()->GetDetailedConnectionStatus(it->second, status, sizeof(status));
        std::string output{status};
        AppendConnectionInfo(output, it->second);
        AppendRealtimeQueueStatus(output, it->second);
        AppendRelayDiagnostics(output);
        return output;
#else
        (void) peer;
        return {};
#endif
    }

    void SteamP2PTransport::QueueEvent(NetworkEvent event) {
        pending_events_.push_back(std::move(event));
    }

    bool SteamP2PTransport::CanAcceptMorePeers() const noexcept {
#if CORE_ENGINE_ENABLE_STEAM
        return conn_to_peer_.size() < max_peers_;
#else
        return false;
#endif
    }

    PeerId SteamP2PTransport::RegisterConnection(std::uint64_t remote_steam_id,
#if CORE_ENGINE_ENABLE_STEAM
                                                 HSteamNetConnection connection,
#else
                                                 std::uint32_t connection,
#endif
                                                 NetworkPeerState state) {
#if CORE_ENGINE_ENABLE_STEAM
        PeerId peer = kInvalidPeerId;
        if (!is_host_) {
            peer = kHostPeerId;
        } else {
            peer = next_peer_id_++;
        }

        conn_to_peer_[connection] = peer;
        peer_to_conn_[peer] = connection;
        conn_to_steam_id_[connection] = remote_steam_id;

        QueueEvent(NetworkEvent{
            .type = state == NetworkPeerState::Connected ? NetworkEventType::PeerConnected : NetworkEventType::PeerConnecting,
            .peer = peer,
            .remote_steam_id = remote_steam_id,
        });
        return peer;
#else
        (void) remote_steam_id;
        (void) connection;
        (void) state;
        return kInvalidPeerId;
#endif
    }

    void SteamP2PTransport::DisconnectConnection(
#if CORE_ENGINE_ENABLE_STEAM
        HSteamNetConnection connection,
#else
        std::uint32_t connection,
#endif
        NetworkDisconnectReason reason) {
#if CORE_ENGINE_ENABLE_STEAM
        const auto peer_it = conn_to_peer_.find(connection);
        const PeerId peer = peer_it != conn_to_peer_.end() ? peer_it->second : kInvalidPeerId;
        const auto steam_it = conn_to_steam_id_.find(connection);
        const std::uint64_t steam_id = steam_it != conn_to_steam_id_.end() ? steam_it->second : 0;

        if (peer != kInvalidPeerId) {
            peer_to_conn_.erase(peer);
        }
        conn_to_peer_.erase(connection);
        conn_to_steam_id_.erase(connection);

        QueueEvent(NetworkEvent{
            .type = NetworkEventType::PeerDisconnected,
            .peer = peer,
            .remote_steam_id = steam_id,
            .disconnect_reason = reason,
        });
#else
        (void) connection;
        (void) reason;
#endif
    }

#if CORE_ENGINE_ENABLE_STEAM
    void SteamP2PTransport::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *callback) {
        if (callback == nullptr || SteamNetworkingSockets() == nullptr) {
            return;
        }

        const HSteamNetConnection connection = callback->m_hConn;
        const std::uint64_t remote_steam_id = callback->m_info.m_identityRemote.GetSteamID64();

        switch (callback->m_info.m_eState) {
            case k_ESteamNetworkingConnectionState_Connecting: {
                const bool known_connection = conn_to_peer_.contains(connection);
                if (!is_host_) {
                    if (!known_connection) {
                        SteamNetworkingSockets()->CloseConnection(connection, 0, "Client cannot accept inbound", false);
                    }
                    return;
                }

                if (known_connection) {
                    return;
                }

                if (!CanAcceptMorePeers()) {
                    SteamNetworkingSockets()->CloseConnection(connection, 1001, "Lobby full", false);
                    return;
                }

                if (SteamNetworkingSockets()->AcceptConnection(connection) != k_EResultOK) {
                    SteamNetworkingSockets()->CloseConnection(connection, 0, "Accept failed", false);
                    return;
                }

                SteamNetworkingSockets()->SetConnectionPollGroup(connection, poll_group_);
                RegisterConnection(remote_steam_id, connection, NetworkPeerState::Connecting);
                break;
            }

            case k_ESteamNetworkingConnectionState_Connected: {
                const auto existing = conn_to_peer_.find(connection);
                if (existing != conn_to_peer_.end()) {
                    QueueEvent(NetworkEvent{
                        .type = NetworkEventType::PeerConnected,
                        .peer = existing->second,
                        .remote_steam_id = remote_steam_id,
                    });
                }
                break;
            }

            case k_ESteamNetworkingConnectionState_ClosedByPeer:
                DisconnectConnection(connection, NetworkDisconnectReason::RemoteClosed);
                SteamNetworkingSockets()->CloseConnection(connection, 0, nullptr, false);
                break;

            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
                DisconnectConnection(connection, NetworkDisconnectReason::TransportError);
                SteamNetworkingSockets()->CloseConnection(connection, 0, nullptr, false);
                break;

            default:
                break;
        }
    }
#endif
} // namespace CoreEngine
