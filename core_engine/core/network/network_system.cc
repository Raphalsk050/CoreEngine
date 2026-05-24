#include "core/network/network_system.h"

#include "core/log/log.h"
#include "core/network/message_reader.h"
#include "core/network/message_writer.h"
#include "core/network/replication/network_snapshot_builder.h"
#include "core/network/transport/local_network_address.h"
#include "core/network/transport/steam_p2p_transport_adapter.h"
#include "core/online/steam/steam_auth_service.h"
#include "core/online/steam/steam_lobby_service.h"
#include "core/online/steam/steam_online_system.h"

#include <algorithm>
#include <chrono>
#include <random>

namespace CoreEngine {
    namespace {
        constexpr std::uint64_t kPingIntervalMs = 500;
        constexpr std::uint64_t kDiagnosticsSummaryIntervalMs = 1000;
        constexpr std::uint64_t kDiagnosticsDetailsIntervalMs = 5000;

        [[nodiscard]] constexpr std::uint32_t HashProtocolString(const char *value) noexcept {
            std::uint32_t hash = 2166136261u;
            while (*value != '\0') {
                hash ^= static_cast<std::uint8_t>(*value++);
                hash *= 16777619u;
            }
            return hash;
        }

        constexpr std::uint32_t kNetworkBuildHash = HashProtocolString("CoreEngine.BountyHunters.Protocol.v4");

        [[nodiscard]] std::uint64_t MakeHandshakeNonce() {
            std::random_device random;
            const std::uint64_t high = static_cast<std::uint64_t>(random()) << 32u;
            const std::uint64_t low = static_cast<std::uint64_t>(random());
            return high | low;
        }

        [[nodiscard]] std::uint64_t NowMilliseconds() noexcept {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        [[nodiscard]] const char *PeerStateName(NetworkPeerState state) noexcept {
            switch (state) {
                case NetworkPeerState::Disconnected:
                    return "Disconnected";
                case NetworkPeerState::Connecting:
                    return "Connecting";
                case NetworkPeerState::Authenticating:
                    return "Authenticating";
                case NetworkPeerState::Connected:
                    return "Connected";
                case NetworkPeerState::Closing:
                    return "Closing";
            }

            return "Unknown";
        }

    }

    NetworkSystem::NetworkSystem(SteamOnlineSystem &online_system)
        : online_system_(online_system) {
    }

    NetworkSystem::~NetworkSystem() {
        Shutdown();
    }

    bool NetworkSystem::Initialize() {
        if (initialized_) {
            return true;
        }

        lobby_service_ = std::make_unique<SteamLobbyService>(online_system_);
        transport_ = std::make_unique<SteamP2PTransportAdapter>(online_system_);
        auth_service_ = std::make_unique<SteamAuthService>(online_system_);
        handshake_nonce_ = MakeHandshakeNonce();
        initialized_ = true;

        if (diagnostics_.Initialize("logs/network_diagnostics.log")) {
            diagnostics_.WriteHeader(online_system_.LocalSteamId(),
                                     QueryLocalNetworkAddressText(),
                                     kNetworkProtocolVersion,
                                     kNetworkBuildHash);
            Log::Info("Network", "Network diagnostics log: {}", diagnostics_.Path().string());
        } else {
            Log::Warn("Network", "Network diagnostics log could not be opened.");
        }

        return true;
    }

    void NetworkSystem::Shutdown() {
        if (!initialized_) {
            return;
        }

        if (transport_ != nullptr) {
            transport_->Shutdown();
            transport_.reset();
        }

        if (auth_service_ != nullptr) {
            auth_service_->Shutdown();
            auth_service_.reset();
        }

        if (lobby_service_ != nullptr) {
            lobby_service_->LeaveLobby();
            lobby_service_.reset();
        }

        current_events_.clear();
        input_commands_.clear();
        gameplay_events_.clear();
        last_input_sequence_by_peer_.clear();
        last_received_sequence_by_peer_.clear();
        pending_ping_sent_time_by_peer_.clear();
        last_protocol_rtt_by_peer_.clear();
        sent_packet_timing_by_sequence_.clear();
        session_.Reset();
        stats_.Reset();
        handshake_nonce_ = 0;
        next_ping_time_ms_ = 0;
        next_diagnostics_summary_time_ms_ = 0;
        next_diagnostics_details_time_ms_ = 0;
        initialized_ = false;
        diagnostics_.LogSessionAction("shutdown", session_);
        diagnostics_.Shutdown();
    }

    void NetworkSystem::BeginFrame() {
        current_events_.clear();
        input_commands_.clear();
        gameplay_events_.clear();
        ++local_tick_;

        if (!initialized_) {
            return;
        }

        NetworkEventQueue events;
        if (lobby_service_ != nullptr) {
            lobby_service_->PollEvents(events);
        }
        if (transport_ != nullptr) {
            transport_->PollEvents(events);
        }
        if (auth_service_ != nullptr) {
            auth_service_->PollEvents(events);
        }

        for (NetworkEvent &event: events) {
            if (event.type == NetworkEventType::PacketReceived && !HandlePacketEvent(event)) {
                ++stats_.packets_dropped;
                diagnostics_.LogMalformedInboundPacket(event.peer, event.remote_steam_id, event.payload.size());
                continue;
            }

            diagnostics_.LogNetworkEvent(event);
            HandleEvent(event);
            current_events_.push_back(std::move(event));
        }

        SendConnectionPings();
        RefreshConnectionMetrics();
        TraceDiagnosticsSummary();
    }

    void NetworkSystem::EndFrame() {
    }

    bool NetworkSystem::CreateFriendsLobby(int max_players) {
        if (!initialized_ || lobby_service_ == nullptr || max_players <= 0) {
            return false;
        }

        requested_max_players_ = max_players;
        ResetSessionRuntimeState(true);
        session_.SetState(NetworkSessionState::CreatingLobby);
        diagnostics_.LogSessionAction("create_friends_lobby_requested", session_);
        return lobby_service_->CreateFriendsLobby(max_players);
    }

    bool NetworkSystem::JoinLobbyById(std::uint64_t lobby_id) {
        if (!initialized_ || lobby_service_ == nullptr || lobby_id == 0) {
            return false;
        }

        ResetSessionRuntimeState(true);
        session_.SetState(NetworkSessionState::Connecting);
        diagnostics_.LogSessionAction("join_lobby_requested", session_);
        return lobby_service_->JoinLobby(lobby_id);
    }

    bool NetworkSystem::CreateDirectHost(std::uint16_t port, int max_players) {
        if (!initialized_ || transport_ == nullptr || port == 0 || max_players <= 0) {
            return false;
        }

        LeaveLobby();
        requested_max_players_ = max_players;
        if (!transport_->StartDirectHost(port, static_cast<std::uint32_t>(max_players))) {
            session_.SetDisconnectReason(NetworkDisconnectReason::TransportError);
            session_.SetState(NetworkSessionState::Disconnecting);
            diagnostics_.LogSessionAction("direct_host_failed", session_);
            return false;
        }

        session_.BeginDirectHost(online_system_.LocalSteamId());
        diagnostics_.LogSessionAction("direct_host_started", session_);
        return true;
    }

    bool NetworkSystem::JoinDirect(std::string_view host, std::uint16_t port) {
        if (!initialized_ || transport_ == nullptr || host.empty() || port == 0) {
            return false;
        }

        LeaveLobby();
        session_.BeginDirectClient(online_system_.LocalSteamId());
        diagnostics_.LogSessionAction("direct_connect_requested", session_);
        if (!transport_->ConnectDirect(host, port)) {
            session_.SetDisconnectReason(NetworkDisconnectReason::TransportError);
            session_.SetState(NetworkSessionState::Disconnecting);
            diagnostics_.LogSessionAction("direct_connect_failed", session_);
            return false;
        }

        return true;
    }

    bool NetworkSystem::OpenInviteOverlay() {
        return initialized_ && lobby_service_ != nullptr && lobby_service_->OpenInviteOverlay();
    }

    void NetworkSystem::LeaveLobby() {
        if (transport_ != nullptr) {
            transport_->Shutdown();
        }
        if (auth_service_ != nullptr) {
            auth_service_->Shutdown();
        }
        if (lobby_service_ != nullptr) {
            lobby_service_->LeaveLobby();
        }

        session_.Reset();
        diagnostics_.LogSessionAction("leave", session_);
        ResetSessionRuntimeState(true);
    }

    bool NetworkSystem::Send(PeerId peer, std::span<const std::byte> payload, SendMode mode) {
        if (!initialized_ || transport_ == nullptr || peer == kInvalidPeerId || payload.empty()) {
            return false;
        }

        const bool sent = transport_->Send(peer, payload, mode);
        diagnostics_.LogOutboundPacket(peer, payload, mode, sent);
        if (sent) {
            ++stats_.packets_out;
            stats_.bytes_out += payload.size();
        } else {
            ++stats_.packets_send_failed;
        }
        return sent;
    }

    bool NetworkSystem::SendPlayerInputCommands(std::span<const PlayerInputCommand> commands) {
        if (!initialized_ ||
            commands.empty() ||
            session_.Role() != NetworkRole::Client ||
            session_.State() != NetworkSessionState::Connected ||
            !IsPeerConnectedForGameplay(kHostPeerId)) {
            return false;
        }

        const std::uint32_t sequence = NextSequence();
        const PeerId peer = kHostPeerId;
        const std::uint64_t now_ms = NowMilliseconds();
        MessageWriter writer;
        if (!writer.Begin(NetMessageType::InputCommand, sequence, LastReceivedSequence(peer), local_tick_) ||
            !WritePlayerInputCommandBatch(writer, commands) ||
            !writer.Finalize()) {
            ++stats_.input_commands_dropped;
            return false;
        }

        const bool sent = Send(peer, writer.Bytes(), SendMode::UnreliableNoDelay);
        if (sent) {
            RecordSentPacket(peer, sequence, now_ms);
        } else {
            ++stats_.input_commands_dropped;
        }
        return sent;
    }

    bool NetworkSystem::SubmitLocalPlayerInputCommand(NetworkEntityId local_entity_id,
                                                      const PlayerInputCommand &command) {
        if (local_entity_id == 0 || session_.Role() == NetworkRole::Client) {
            return false;
        }

        input_commands_.push_back(QueuedPlayerInputCommand{
            .peer = kInvalidPeerId,
            .player_network_id = local_entity_id,
            .remote_user_id = local_entity_id,
            .command = command,
        });
        return true;
    }

    bool NetworkSystem::SendGameplayEvent(PeerId peer,
                                          const NetworkGameplayEvent &event,
                                          SendMode mode) {
        if (!initialized_ ||
            event.event_type == 0u ||
            event.payload_size > kMaxNetworkGameplayEventPayloadBytes ||
            session_.State() != NetworkSessionState::Connected ||
            !IsPeerConnectedForGameplay(peer)) {
            return false;
        }

        const std::uint32_t sequence = NextSequence();
        const std::uint64_t now_ms = NowMilliseconds();
        MessageWriter writer(160);
        if (!writer.Begin(NetMessageType::GameplayEvent, sequence, LastReceivedSequence(peer), local_tick_) ||
            !writer.WriteUInt32(event.event_type) ||
            !writer.WriteUInt64(event.source_network_id) ||
            !writer.WriteUInt32(event.sequence) ||
            !writer.WriteUInt32(event.server_tick) ||
            !writer.WriteSizedBytes(event.Payload()) ||
            !writer.Finalize()) {
            return false;
        }

        const bool sent = Send(peer, writer.Bytes(), mode);
        if (sent) {
            RecordSentPacket(peer, sequence, now_ms);
        }
        return sent;
    }

    bool NetworkSystem::SendGameplayEventToHost(const NetworkGameplayEvent &event,
                                                SendMode mode) {
        if (session_.Role() != NetworkRole::Client) {
            return false;
        }

        return SendGameplayEvent(kHostPeerId, event, mode);
    }

    bool NetworkSystem::BroadcastGameplayEvent(const NetworkGameplayEvent &event,
                                               PeerId excluded_peer,
                                               SendMode mode) {
        if (session_.Role() != NetworkRole::Host ||
            session_.State() != NetworkSessionState::Connected) {
            return false;
        }

        bool sent_any = false;
        for (const NetworkPeer &peer: session_.Peers()) {
            if (peer.id == excluded_peer || peer.state != NetworkPeerState::Connected) {
                continue;
            }

            sent_any = SendGameplayEvent(peer.id, event, mode) || sent_any;
        }

        return sent_any;
    }

    bool NetworkSystem::SendWorldSnapshot(PeerId peer,
                                          std::span<const NetworkTransformSnapshot> transforms,
                                          std::uint32_t server_tick,
                                          std::uint32_t snapshot_sequence,
                                          std::uint32_t last_processed_input_sequence) {
        if (!initialized_ || transforms.empty()) {
            return false;
        }

        const std::uint32_t sequence = NextSequence();
        const std::uint64_t now_ms = NowMilliseconds();
        MessageWriter writer(2048);
        NetworkSnapshotBuilder builder;
        NetworkSnapshotBuildResult build_result;
        if (!writer.Begin(NetMessageType::WorldSnapshot, sequence, LastReceivedSequence(peer), server_tick) ||
            !builder.BuildTransformSnapshot(
                NetworkSnapshotBuildDesc{
                    .server_tick = server_tick,
                    .snapshot_sequence = snapshot_sequence,
                    .last_processed_input_sequence = last_processed_input_sequence,
                },
                transforms,
                writer,
                build_result) ||
            !writer.Finalize()) {
            ++stats_.snapshots_dropped;
            return false;
        }

        const bool sent = Send(peer, writer.Bytes(), SendMode::UnreliableNoDelay);
        if (sent) {
            RecordSentPacket(peer, sequence, now_ms);
            ++stats_.snapshots_sent;
            const std::uint64_t previous_count = stats_.snapshots_sent - 1u;
            const auto packet_size = static_cast<float>(writer.Bytes().size());
            const float average =
                previous_count == 0u
                    ? packet_size
                    : ((stats_.avg_snapshot_size_bytes * static_cast<float>(previous_count)) + packet_size) /
                          static_cast<float>(stats_.snapshots_sent);
            stats_.avg_snapshot_size_bytes = static_cast<std::uint32_t>(average + 0.5f);
            stats_.last_snapshot_tick = server_tick;
        } else {
            ++stats_.snapshots_dropped;
        }

        return sent;
    }

    void NetworkSystem::DumpConnectionStatus() const {
        if (transport_ == nullptr) {
            return;
        }

        for (const NetworkPeer &peer: session_.Peers()) {
            const std::string status = transport_->DetailedConnectionStatus(peer.id);
            if (!status.empty()) {
                Log::Info("Network", "Peer {} connection status:\n{}", peer.id, status);
            }
        }
    }

    std::string NetworkSystem::ConnectionDiagnosticsText() const {
        if (transport_ == nullptr) {
            return {};
        }

        std::string output;
        for (const NetworkPeer &peer: session_.Peers()) {
            if (peer.state != NetworkPeerState::Connected &&
                peer.state != NetworkPeerState::Authenticating &&
                peer.state != NetworkPeerState::Connecting) {
                continue;
            }

            if (!output.empty()) {
                output.append("\n\n");
            }

            output.append("Peer ");
            output.append(std::to_string(peer.id));
            output.append(" steam_id=");
            output.append(std::to_string(peer.steam_id));
            output.append(" state=");
            output.append(PeerStateName(peer.state));
            output.append("\n");

            const std::string status = transport_->DetailedConnectionStatus(peer.id);
            output.append(status.empty() ? "No Steam connection diagnostics available." : status);
        }

        return output;
    }

    std::span<const SteamLobbyMember> NetworkSystem::LobbyMembers() const noexcept {
        return lobby_service_ != nullptr ? lobby_service_->Members() : std::span<const SteamLobbyMember>{};
    }

    std::uint32_t NetworkSystem::LastProcessedInputSequence(PeerId peer) const noexcept {
        const auto it = last_input_sequence_by_peer_.find(peer);
        return it != last_input_sequence_by_peer_.end() ? it->second : 0u;
    }

    std::string NetworkSystem::DetailedConnectionStatus(PeerId peer) const {
        return transport_ != nullptr ? transport_->DetailedConnectionStatus(peer) : std::string{};
    }

    std::string NetworkSystem::LocalNetworkAddressText() const {
        return QueryLocalNetworkAddressText();
    }

    void NetworkSystem::RecordPredictionCorrection(ReconciliationAction action,
                                                   float position_error,
                                                   std::uint32_t confirmed_sequence,
                                                   std::uint32_t latest_sequence,
                                                   std::size_t replay_count) {
        if (action == ReconciliationAction::SmoothCorrection) {
            ++stats_.prediction_corrections;
        } else if (action == ReconciliationAction::HardSnap) {
            ++stats_.prediction_hard_snaps;
        }

        if (action != ReconciliationAction::None) {
            diagnostics_.LogPredictionCorrection(action,
                                                 position_error,
                                                 confirmed_sequence,
                                                 latest_sequence,
                                                 replay_count);
        }
    }

    void NetworkSystem::HandleEvent(NetworkEvent &event) {
        switch (event.type) {
            case NetworkEventType::LobbyCreated:
                session_.BeginHostLobby(event.lobby_id, online_system_.LocalSteamId());
                if (transport_ != nullptr && !transport_->StartHost(0, static_cast<std::uint32_t>(requested_max_players_))) {
                    session_.SetDisconnectReason(NetworkDisconnectReason::TransportError);
                    session_.SetState(NetworkSessionState::Disconnecting);
                }
                diagnostics_.LogSessionAction("lobby_created", session_);
                break;

            case NetworkEventType::LobbyEntered:
                if (event.lobby_owner_id == online_system_.LocalSteamId()) {
                    session_.BeginHostLobby(event.lobby_id, online_system_.LocalSteamId());
                    diagnostics_.LogSessionAction("lobby_entered_as_host", session_);
                    break;
                }

                session_.BeginClientLobby(event.lobby_id, event.lobby_owner_id, online_system_.LocalSteamId());
                if (transport_ != nullptr && !transport_->ConnectToHost(event.lobby_owner_id, 0)) {
                    session_.SetDisconnectReason(NetworkDisconnectReason::TransportError);
                    session_.SetState(NetworkSessionState::Disconnecting);
                }
                diagnostics_.LogSessionAction("lobby_entered_as_client", session_);
                break;

            case NetworkEventType::LobbyJoinRequested:
                JoinLobbyById(event.lobby_id);
                break;

            case NetworkEventType::LobbyLeft:
                LeaveLobby();
                break;

            case NetworkEventType::LobbyOwnerChanged:
                if (event.lobby_id == 0 || event.lobby_id != session_.LobbyId()) {
                    break;
                }

                if (event.lobby_owner_id == online_system_.LocalSteamId()) {
                    session_.BeginHostLobby(event.lobby_id, online_system_.LocalSteamId());
                    if (transport_ != nullptr &&
                        !transport_->StartHost(0, static_cast<std::uint32_t>(requested_max_players_))) {
                        session_.SetDisconnectReason(NetworkDisconnectReason::TransportError);
                        session_.SetState(NetworkSessionState::Disconnecting);
                    }
                    diagnostics_.LogSessionAction("lobby_owner_changed_to_local", session_);
                    break;
                }

                session_.BeginClientLobby(event.lobby_id, event.lobby_owner_id, online_system_.LocalSteamId());
                if (transport_ != nullptr && !transport_->ConnectToHost(event.lobby_owner_id, 0)) {
                    session_.SetDisconnectReason(NetworkDisconnectReason::TransportError);
                    session_.SetState(NetworkSessionState::Disconnecting);
                }
                diagnostics_.LogSessionAction("lobby_owner_changed_to_remote", session_);
                break;

            case NetworkEventType::PeerConnecting:
                session_.AddOrUpdatePeer(event.peer, event.remote_steam_id, NetworkPeerState::Connecting);
                diagnostics_.LogSessionAction("peer_connecting", session_);
                break;

            case NetworkEventType::PeerConnected:
                session_.AddOrUpdatePeer(event.peer, event.remote_steam_id, NetworkPeerState::Authenticating);
                session_.SetState(NetworkSessionState::Authenticating);
                if (session_.Role() == NetworkRole::Host) {
                    SendHello(event.peer, NetMessageType::ServerHello);
                } else {
                    SendHello(event.peer, NetMessageType::ClientHello);
                }
                SendAuthTicket(event.peer);
                diagnostics_.LogSessionAction("peer_connected_authenticating", session_);
                break;

            case NetworkEventType::PeerDisconnected:
                session_.RemovePeer(event.peer);
                session_.SetDisconnectReason(event.disconnect_reason);
                if (session_.Role() == NetworkRole::Client && event.peer == kHostPeerId) {
                    session_.SetState(NetworkSessionState::Disconnecting);
                }
                diagnostics_.LogSessionAction("peer_disconnected", session_);
                break;

            case NetworkEventType::AuthAccepted:
                session_.AddOrUpdatePeer(event.peer, event.remote_steam_id, NetworkPeerState::Connected);
                session_.SetState(NetworkSessionState::Connected);
                SendAuthAccepted(event.peer);
                diagnostics_.LogSessionAction("auth_accepted", session_);
                break;

            case NetworkEventType::AuthRejected:
                session_.AddOrUpdatePeer(event.peer, event.remote_steam_id, NetworkPeerState::Closing);
                session_.SetDisconnectReason(event.disconnect_reason);
                SendAuthRejected(event.peer, event.disconnect_reason);
                diagnostics_.LogSessionAction("auth_rejected", session_);
                break;

            case NetworkEventType::PacketReceived:
                HandleProtocolMessage(event);
                break;

            case NetworkEventType::None:
                break;
        }
    }

    bool NetworkSystem::HandlePacketEvent(NetworkEvent &event) {
        ++stats_.packets_in;
        stats_.bytes_in += event.payload.size();

        PacketHeader header;
        std::span<const std::byte> payload;
        if (!ParsePacket(event.payload, header, payload)) {
            return false;
        }

        event.message_type = header.message_type;
        event.sequence = header.sequence;
        event.ack = header.ack;
        event.tick = header.tick;
        const std::size_t packet_size = event.payload.size();
        event.payload.assign(payload.begin(), payload.end());
        diagnostics_.LogInboundPacket(event, packet_size);

        if (event.peer != kInvalidPeerId) {
            std::uint32_t &last_received = last_received_sequence_by_peer_[event.peer];
            if (header.sequence > last_received) {
                last_received = header.sequence;
            }
            HandlePacketAck(event.peer, header.ack, NowMilliseconds());
        }

        if (event.message_type == NetMessageType::InputCommand) {
            stats_.last_input_tick = header.tick;
        } else if (event.message_type == NetMessageType::WorldSnapshot) {
            stats_.last_snapshot_tick = header.tick;
        }

        return true;
    }

    void NetworkSystem::HandleProtocolMessage(const NetworkEvent &event) {
        switch (event.message_type) {
            case NetMessageType::ClientHello:
            case NetMessageType::ServerHello: {
                MessageReader reader(event.payload);
                std::uint64_t steam_id = 0;
                std::uint16_t protocol = 0;
                std::uint32_t build_hash = 0;
                std::uint64_t nonce = 0;

                if (!reader.ReadUInt64(steam_id) ||
                    !reader.ReadUInt16(protocol) ||
                    !reader.ReadUInt32(build_hash) ||
                    !reader.ReadUInt64(nonce) ||
                    protocol != kNetworkProtocolVersion ||
                    build_hash != kNetworkBuildHash) {
                    SendAuthRejected(event.peer, NetworkDisconnectReason::ProtocolMismatch);
                    return;
                }

                (void) nonce;
                session_.AddOrUpdatePeer(event.peer, steam_id, NetworkPeerState::Authenticating);
                break;
            }

            case NetMessageType::AuthTicket: {
                MessageReader reader(event.payload);
                std::uint64_t steam_id = 0;
                std::span<const std::byte> ticket;
                if (!reader.ReadUInt64(steam_id) || !reader.ReadSizedBytes(ticket)) {
                    SendAuthRejected(event.peer, NetworkDisconnectReason::AuthenticationFailed);
                    return;
                }

                if (auth_service_ == nullptr || !auth_service_->BeginAuthSession(event.peer, steam_id, ticket)) {
                    SendAuthRejected(event.peer, NetworkDisconnectReason::AuthenticationFailed);
                }
                break;
            }

            case NetMessageType::AuthAccepted:
                session_.AddOrUpdatePeer(
                    event.peer,
                    RemoteUserIdForPeer(event.peer, event.remote_steam_id),
                    NetworkPeerState::Connected);
                session_.SetState(NetworkSessionState::Connected);
                break;

            case NetMessageType::AuthRejected:
                session_.SetDisconnectReason(NetworkDisconnectReason::AuthenticationFailed);
                session_.SetState(NetworkSessionState::Disconnecting);
                break;

            case NetMessageType::Ping:
                HandlePingMessage(event);
                break;

            case NetMessageType::Pong:
                HandlePongMessage(event);
                break;

            case NetMessageType::InputCommand:
                HandleInputCommandMessage(event);
                break;

            case NetMessageType::WorldSnapshot:
                ++stats_.snapshots_received;
                break;

            case NetMessageType::GameplayEvent:
                HandleGameplayEventMessage(event);
                break;

            case NetMessageType::EntitySpawn:
            case NetMessageType::EntityDespawn:
            case NetMessageType::Disconnect:
                break;
        }
    }

    bool NetworkSystem::SendEmptyMessage(PeerId peer, NetMessageType type, SendMode mode) {
        const std::uint32_t sequence = NextSequence();
        const std::uint64_t now_ms = NowMilliseconds();
        MessageWriter writer;
        if (!writer.Begin(type, sequence, LastReceivedSequence(peer), local_tick_) || !writer.Finalize()) {
            return false;
        }

        const bool sent = Send(peer, writer.Bytes(), mode);
        if (sent) {
            RecordSentPacket(peer, sequence, now_ms);
        }
        return sent;
    }

    bool NetworkSystem::SendHello(PeerId peer, NetMessageType type) {
        const std::uint32_t sequence = NextSequence();
        const std::uint64_t now_ms = NowMilliseconds();
        MessageWriter writer;
        if (!writer.Begin(type, sequence, LastReceivedSequence(peer), local_tick_) ||
            !writer.WriteUInt64(online_system_.LocalSteamId()) ||
            !writer.WriteUInt16(kNetworkProtocolVersion) ||
            !writer.WriteUInt32(kNetworkBuildHash) ||
            !writer.WriteUInt64(handshake_nonce_) ||
            !writer.Finalize()) {
            return false;
        }

        const bool sent = Send(peer, writer.Bytes(), SendMode::ReliableNoNagle);
        if (sent) {
            RecordSentPacket(peer, sequence, now_ms);
        }
        return sent;
    }

    bool NetworkSystem::SendAuthTicket(PeerId peer) {
        if (auth_service_ == nullptr) {
            return false;
        }

        if (auth_service_->LocalTicket().empty() && !auth_service_->CreateLocalTicket()) {
            return false;
        }

        const std::uint32_t sequence = NextSequence();
        const std::uint64_t now_ms = NowMilliseconds();
        MessageWriter writer;
        if (!writer.Begin(NetMessageType::AuthTicket, sequence, LastReceivedSequence(peer), local_tick_) ||
            !writer.WriteUInt64(online_system_.LocalSteamId()) ||
            !writer.WriteSizedBytes(auth_service_->LocalTicket()) ||
            !writer.Finalize()) {
            return false;
        }

        const bool sent = Send(peer, writer.Bytes(), SendMode::ReliableNoNagle);
        if (sent) {
            RecordSentPacket(peer, sequence, now_ms);
        }
        return sent;
    }

    bool NetworkSystem::SendAuthAccepted(PeerId peer) {
        return SendEmptyMessage(peer, NetMessageType::AuthAccepted, SendMode::ReliableNoNagle);
    }

    bool NetworkSystem::SendAuthRejected(PeerId peer, NetworkDisconnectReason reason) {
        const std::uint32_t sequence = NextSequence();
        const std::uint64_t now_ms = NowMilliseconds();
        MessageWriter writer;
        if (!writer.Begin(NetMessageType::AuthRejected, sequence, LastReceivedSequence(peer), local_tick_) ||
            !writer.WriteUInt16(static_cast<std::uint16_t>(reason)) ||
            !writer.Finalize()) {
            return false;
        }

        const bool sent = Send(peer, writer.Bytes(), SendMode::ReliableNoNagle);
        if (sent) {
            RecordSentPacket(peer, sequence, now_ms);
        }
        return sent;
    }

    void NetworkSystem::SendConnectionPings() {
        if (!initialized_ || session_.State() != NetworkSessionState::Connected) {
            return;
        }

        const std::uint64_t now_ms = NowMilliseconds();
        if (now_ms < next_ping_time_ms_) {
            return;
        }

        next_ping_time_ms_ = now_ms + kPingIntervalMs;
        for (const NetworkPeer &peer: session_.Peers()) {
            if (peer.state == NetworkPeerState::Connected) {
                SendPing(peer.id, now_ms);
            }
        }
    }

    void NetworkSystem::RefreshConnectionMetrics() noexcept {
        if (!initialized_ || transport_ == nullptr || session_.State() != NetworkSessionState::Connected) {
            stats_.ping_ms = -1;
            stats_.jitter_ms = 0;
            stats_.protocol_ping_ms = -1;
            stats_.protocol_jitter_ms = 0;
            stats_.transport_queue_time_ms = 0;
            stats_.transport_pending_unreliable_bytes = 0;
            stats_.transport_pending_reliable_bytes = 0;
            stats_.transport_send_rate_bytes_per_second = 0;
            stats_.packet_loss = 0.0f;
            last_protocol_rtt_by_peer_.clear();
            pending_ping_sent_time_by_peer_.clear();
            sent_packet_timing_by_sequence_.clear();
            return;
        }

        int ping_total = 0;
        int ping_count = 0;
        int max_jitter = 0;
        int max_queue_time = 0;
        std::uint32_t pending_unreliable_bytes = 0;
        std::uint32_t pending_reliable_bytes = 0;
        std::uint32_t send_rate_bytes_per_second = 0;
        float max_packet_loss = 0.0f;
        bool has_connected_peer = false;

        for (const NetworkPeer &peer: session_.Peers()) {
            if (peer.state != NetworkPeerState::Connected) {
                continue;
            }

            has_connected_peer = true;
            NetworkConnectionMetrics metrics;
            if (!transport_->QueryMetrics(peer.id, metrics) || !metrics.valid) {
                continue;
            }

            ping_total += metrics.ping_ms;
            ++ping_count;
            max_jitter = std::max(max_jitter, metrics.jitter_ms);
            max_queue_time = std::max(max_queue_time, metrics.queue_time_ms);
            pending_unreliable_bytes += metrics.pending_unreliable_bytes;
            pending_reliable_bytes += metrics.pending_reliable_bytes;
            send_rate_bytes_per_second = std::max(send_rate_bytes_per_second, metrics.send_rate_bytes_per_second);
            max_packet_loss = std::max(max_packet_loss, metrics.packet_loss);
        }

        if (!has_connected_peer) {
            stats_.ping_ms = -1;
            stats_.jitter_ms = 0;
            stats_.protocol_ping_ms = -1;
            stats_.protocol_jitter_ms = 0;
            stats_.transport_queue_time_ms = 0;
            stats_.transport_pending_unreliable_bytes = 0;
            stats_.transport_pending_reliable_bytes = 0;
            stats_.transport_send_rate_bytes_per_second = 0;
            stats_.packet_loss = 0.0f;
            last_protocol_rtt_by_peer_.clear();
            pending_ping_sent_time_by_peer_.clear();
            sent_packet_timing_by_sequence_.clear();
            return;
        }

        if (ping_count > 0) {
            stats_.ping_ms = ping_total / ping_count;
            stats_.jitter_ms = max_jitter;
            stats_.transport_queue_time_ms = max_queue_time;
            stats_.transport_pending_unreliable_bytes = pending_unreliable_bytes;
            stats_.transport_pending_reliable_bytes = pending_reliable_bytes;
            stats_.transport_send_rate_bytes_per_second = send_rate_bytes_per_second;
            stats_.packet_loss = max_packet_loss;
        } else {
            stats_.ping_ms = -1;
            stats_.jitter_ms = 0;
            stats_.transport_queue_time_ms = 0;
            stats_.transport_pending_unreliable_bytes = 0;
            stats_.transport_pending_reliable_bytes = 0;
            stats_.transport_send_rate_bytes_per_second = 0;
            stats_.packet_loss = 0.0f;
        }
    }

    bool NetworkSystem::SendPing(PeerId peer, std::uint64_t now_ms) {
        const std::uint32_t sequence = NextSequence();
        MessageWriter writer;
        if (!writer.Begin(NetMessageType::Ping, sequence, LastReceivedSequence(peer), local_tick_) ||
            !writer.WriteUInt64(now_ms) ||
            !writer.Finalize()) {
            return false;
        }

        if (!Send(peer, writer.Bytes(), SendMode::UnreliableNoDelay)) {
            return false;
        }

        RecordSentPacket(peer, sequence, now_ms);
        pending_ping_sent_time_by_peer_[peer] = now_ms;
        return true;
    }

    bool NetworkSystem::SendPong(PeerId peer, std::uint64_t ping_sent_time_ms) {
        const std::uint32_t sequence = NextSequence();
        const std::uint64_t now_ms = NowMilliseconds();
        MessageWriter writer;
        if (!writer.Begin(NetMessageType::Pong, sequence, LastReceivedSequence(peer), local_tick_) ||
            !writer.WriteUInt64(ping_sent_time_ms) ||
            !writer.Finalize()) {
            return false;
        }

        const bool sent = Send(peer, writer.Bytes(), SendMode::UnreliableNoDelay);
        if (sent) {
            RecordSentPacket(peer, sequence, now_ms);
        }
        return sent;
    }

    void NetworkSystem::HandlePingMessage(const NetworkEvent &event) {
        MessageReader reader(event.payload);
        std::uint64_t ping_sent_time_ms = 0;
        if (!reader.ReadUInt64(ping_sent_time_ms) || reader.Remaining() != 0u) {
            return;
        }

        SendPong(event.peer, ping_sent_time_ms);
    }

    void NetworkSystem::HandlePongMessage(const NetworkEvent &event) {
        MessageReader reader(event.payload);
        std::uint64_t ping_sent_time_ms = 0;
        if (!reader.ReadUInt64(ping_sent_time_ms) || reader.Remaining() != 0u) {
            return;
        }

        const auto pending = pending_ping_sent_time_by_peer_.find(event.peer);
        if (pending == pending_ping_sent_time_by_peer_.end() ||
            pending->second != ping_sent_time_ms) {
            return;
        }

        pending_ping_sent_time_by_peer_.erase(pending);
        UpdateRoundTripStats(event.peer, ping_sent_time_ms, NowMilliseconds());
    }

    void NetworkSystem::HandleInputCommandMessage(const NetworkEvent &event) {
        if (session_.Role() != NetworkRole::Host ||
            session_.State() != NetworkSessionState::Connected ||
            !IsPeerConnectedForGameplay(event.peer)) {
            ++stats_.input_commands_dropped;
            return;
        }

        MessageReader reader(event.payload);
        PlayerInputCommandBatch batch;
        if (!ReadPlayerInputCommandBatch(reader, batch)) {
            ++stats_.input_commands_dropped;
            return;
        }

        std::uint32_t &last_sequence = last_input_sequence_by_peer_[event.peer];
        const std::uint64_t remote_user_id = RemoteUserIdForPeer(event.peer, event.remote_steam_id);
        for (std::uint8_t i = 0; i < batch.count; ++i) {
            const PlayerInputCommand &command = batch.commands[i];
            if (command.sequence <= last_sequence) {
                ++stats_.input_commands_duplicated;
                continue;
            }

            last_sequence = command.sequence;
            input_commands_.push_back(QueuedPlayerInputCommand{
                .peer = event.peer,
                .player_network_id = MakeNetworkPlayerEntityId(event.peer, remote_user_id),
                .remote_user_id = remote_user_id,
                .command = command,
            });
            ++stats_.input_commands_received;
        }
    }

    void NetworkSystem::HandleGameplayEventMessage(const NetworkEvent &event) {
        if (!IsPeerConnectedForGameplay(event.peer)) {
            ++stats_.packets_dropped;
            return;
        }

        MessageReader reader(event.payload);
        NetworkGameplayEvent gameplay_event;
        std::span<const std::byte> payload;
        if (!reader.ReadUInt32(gameplay_event.event_type) ||
            !reader.ReadUInt64(gameplay_event.source_network_id) ||
            !reader.ReadUInt32(gameplay_event.sequence) ||
            !reader.ReadUInt32(gameplay_event.server_tick) ||
            !reader.ReadSizedBytes(payload) ||
            reader.Remaining() != 0u ||
            gameplay_event.event_type == 0u ||
            payload.size() > kMaxNetworkGameplayEventPayloadBytes ||
            !gameplay_event.SetPayload(payload)) {
            ++stats_.packets_dropped;
            return;
        }

        gameplay_event.peer = event.peer;
        gameplay_events_.push_back(gameplay_event);
    }

    std::uint64_t NetworkSystem::RemoteUserIdForPeer(PeerId peer, std::uint64_t fallback_user_id) const noexcept {
        if (const NetworkPeer *known_peer = session_.FindPeer(peer);
            known_peer != nullptr && known_peer->steam_id != 0) {
            return known_peer->steam_id;
        }

        return fallback_user_id;
    }

    bool NetworkSystem::IsPeerConnectedForGameplay(PeerId peer) const noexcept {
        if (peer == kInvalidPeerId) {
            return false;
        }

        const NetworkPeer *known_peer = session_.FindPeer(peer);
        return known_peer != nullptr && known_peer->state == NetworkPeerState::Connected;
    }

    void NetworkSystem::ResetSessionRuntimeState(bool reset_stats) noexcept {
        current_events_.clear();
        input_commands_.clear();
        gameplay_events_.clear();
        last_input_sequence_by_peer_.clear();
        last_received_sequence_by_peer_.clear();
        pending_ping_sent_time_by_peer_.clear();
        last_protocol_rtt_by_peer_.clear();
        sent_packet_timing_by_sequence_.clear();
        next_ping_time_ms_ = 0;
        next_sequence_ = 1;

        if (reset_stats) {
            stats_.Reset();
        }

        stats_.ping_ms = -1;
        stats_.jitter_ms = 0;
        stats_.protocol_ping_ms = -1;
        stats_.protocol_jitter_ms = 0;
        stats_.transport_queue_time_ms = 0;
        stats_.transport_pending_unreliable_bytes = 0;
        stats_.transport_pending_reliable_bytes = 0;
        stats_.transport_send_rate_bytes_per_second = 0;
        stats_.packet_loss = 0.0f;
    }

    void NetworkSystem::TraceDiagnosticsSummary() {
        if (!diagnostics_.IsOpen()) {
            return;
        }

        const std::uint64_t now_ms = NowMilliseconds();
        if (now_ms < next_diagnostics_summary_time_ms_) {
            return;
        }

        next_diagnostics_summary_time_ms_ = now_ms + kDiagnosticsSummaryIntervalMs;
        diagnostics_.LogSummary(session_,
                                stats_,
                                NetworkDiagnosticsRuntimeState{
                                    .local_tick = local_tick_,
                                    .pending_packet_acks = sent_packet_timing_by_sequence_.size(),
                                    .pending_protocol_pings = pending_ping_sent_time_by_peer_.size(),
                                    .pending_transport_packets = stats_.transport_pending_unreliable_bytes +
                                                                 stats_.transport_pending_reliable_bytes,
                                });

        if (ShouldLogDetailedConnectionDiagnostics(now_ms)) {
            diagnostics_.LogConnectionDetails(ConnectionDiagnosticsText());
        }
    }

    bool NetworkSystem::ShouldLogDetailedConnectionDiagnostics(std::uint64_t now_ms) noexcept {
        if (session_.State() != NetworkSessionState::Connected) {
            return false;
        }

        const bool suspicious =
            stats_.transport_queue_time_ms > 8 ||
            stats_.transport_pending_unreliable_bytes > 0 ||
            stats_.transport_pending_reliable_bytes > 0 ||
            stats_.packet_loss > 0.0f ||
            stats_.packets_send_failed > 0 ||
            stats_.packets_dropped > 0 ||
            stats_.input_commands_dropped > 0 ||
            stats_.snapshots_dropped > 0 ||
            stats_.prediction_hard_snaps > 0 ||
            (stats_.ping_ms >= 0 &&
             stats_.protocol_ping_ms >= 0 &&
             stats_.protocol_ping_ms > stats_.ping_ms + 16);

        if (now_ms < next_diagnostics_details_time_ms_ && !suspicious) {
            return false;
        }

        next_diagnostics_details_time_ms_ =
            now_ms + (suspicious ? kDiagnosticsSummaryIntervalMs : kDiagnosticsDetailsIntervalMs);
        return true;
    }

    void NetworkSystem::RecordSentPacket(PeerId peer, std::uint32_t sequence, std::uint64_t now_ms) {
        if (peer == kInvalidPeerId || sequence == 0) {
            return;
        }

        sent_packet_timing_by_sequence_[sequence] = SentPacketTiming{
            .peer = peer,
            .sent_time_ms = now_ms,
        };

        if (sent_packet_timing_by_sequence_.size() <= 512u) {
            return;
        }

        std::erase_if(sent_packet_timing_by_sequence_, [now_ms](const auto &entry) {
            return now_ms > entry.second.sent_time_ms + 5000u;
        });
    }

    void NetworkSystem::HandlePacketAck(PeerId peer, std::uint32_t ack, std::uint64_t now_ms) noexcept {
        if (peer == kInvalidPeerId || ack == 0) {
            return;
        }

        const auto sent = sent_packet_timing_by_sequence_.find(ack);
        if (sent == sent_packet_timing_by_sequence_.end() || sent->second.peer != peer) {
            return;
        }

        UpdateRoundTripStats(peer, sent->second.sent_time_ms, now_ms);
        sent_packet_timing_by_sequence_.erase(sent);
    }

    void NetworkSystem::UpdateRoundTripStats(PeerId peer,
                                             std::uint64_t ping_sent_time_ms,
                                             std::uint64_t now_ms) noexcept {
        if (now_ms < ping_sent_time_ms) {
            return;
        }

        const int rtt_ms = static_cast<int>(std::min<std::uint64_t>(
            now_ms - ping_sent_time_ms,
            static_cast<std::uint64_t>(INT32_MAX)));

        const auto previous = last_protocol_rtt_by_peer_.find(peer);
        if (previous != last_protocol_rtt_by_peer_.end()) {
            const int delta = std::abs(rtt_ms - previous->second);
            stats_.protocol_jitter_ms =
                stats_.protocol_jitter_ms == 0 ? delta : ((stats_.protocol_jitter_ms * 3) + delta) / 4;
            previous->second = rtt_ms;
        } else {
            last_protocol_rtt_by_peer_[peer] = rtt_ms;
            stats_.protocol_jitter_ms = 0;
        }

        stats_.protocol_ping_ms = rtt_ms;
    }

    std::uint32_t NetworkSystem::LastReceivedSequence(PeerId peer) const noexcept {
        const auto it = last_received_sequence_by_peer_.find(peer);
        return it != last_received_sequence_by_peer_.end() ? it->second : 0u;
    }
} // namespace CoreEngine
