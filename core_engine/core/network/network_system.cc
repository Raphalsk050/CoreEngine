#include "core/network/network_system.h"

#include "core/log/log.h"
#include "core/network/message_reader.h"
#include "core/network/message_writer.h"
#include "core/network/replication/network_snapshot_builder.h"
#include "core/network/transport/steam_p2p_transport_adapter.h"
#include "core/online/steam/steam_auth_service.h"
#include "core/online/steam/steam_lobby_service.h"
#include "core/online/steam/steam_online_system.h"

#include <random>

namespace CoreEngine {
    namespace {
        [[nodiscard]] constexpr std::uint32_t HashProtocolString(const char *value) noexcept {
            std::uint32_t hash = 2166136261u;
            while (*value != '\0') {
                hash ^= static_cast<std::uint8_t>(*value++);
                hash *= 16777619u;
            }
            return hash;
        }

        constexpr std::uint32_t kNetworkBuildHash = HashProtocolString("CoreEngine.BountyHunters.Protocol.v1");

        [[nodiscard]] std::uint64_t MakeHandshakeNonce() {
            std::random_device random;
            const std::uint64_t high = static_cast<std::uint64_t>(random()) << 32u;
            const std::uint64_t low = static_cast<std::uint64_t>(random());
            return high | low;
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
        last_input_sequence_by_peer_.clear();
        session_.Reset();
        stats_.Reset();
        handshake_nonce_ = 0;
        initialized_ = false;
    }

    void NetworkSystem::BeginFrame() {
        current_events_.clear();
        input_commands_.clear();
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
                continue;
            }

            HandleEvent(event);
            current_events_.push_back(std::move(event));
        }
    }

    void NetworkSystem::EndFrame() {
    }

    bool NetworkSystem::CreateFriendsLobby(int max_players) {
        if (!initialized_ || lobby_service_ == nullptr || max_players <= 0) {
            return false;
        }

        requested_max_players_ = max_players;
        session_.SetState(NetworkSessionState::CreatingLobby);
        return lobby_service_->CreateFriendsLobby(max_players);
    }

    bool NetworkSystem::JoinLobbyById(std::uint64_t lobby_id) {
        if (!initialized_ || lobby_service_ == nullptr || lobby_id == 0) {
            return false;
        }

        session_.SetState(NetworkSessionState::Connecting);
        return lobby_service_->JoinLobby(lobby_id);
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
        current_events_.clear();
    }

    bool NetworkSystem::Send(PeerId peer, std::span<const std::byte> payload, SendMode mode) {
        if (!initialized_ || transport_ == nullptr || peer == kInvalidPeerId || payload.empty()) {
            return false;
        }

        const bool sent = transport_->Send(peer, payload, mode);
        if (sent) {
            ++stats_.packets_out;
            stats_.bytes_out += payload.size();
        }
        return sent;
    }

    bool NetworkSystem::SendPlayerInputCommands(std::span<const PlayerInputCommand> commands) {
        if (!initialized_ || commands.empty() || session_.Role() != NetworkRole::Client) {
            return false;
        }

        MessageWriter writer;
        if (!writer.Begin(NetMessageType::InputCommand, NextSequence(), 0, local_tick_) ||
            !WritePlayerInputCommandBatch(writer, commands) ||
            !writer.Finalize()) {
            ++stats_.input_commands_dropped;
            return false;
        }

        return Send(kHostPeerId, writer.Bytes(), SendMode::UnreliableNoDelay);
    }

    bool NetworkSystem::SubmitLocalPlayerInputCommand(NetworkEntityId local_entity_id,
                                                      const PlayerInputCommand &command) {
        if (local_entity_id == 0 || session_.Role() == NetworkRole::Client) {
            return false;
        }

        input_commands_.push_back(QueuedPlayerInputCommand{
            .peer = kInvalidPeerId,
            .remote_user_id = local_entity_id,
            .command = command,
        });
        ++stats_.input_commands_received;
        return true;
    }

    bool NetworkSystem::SendWorldSnapshot(PeerId peer,
                                          std::span<const NetworkTransformSnapshot> transforms,
                                          std::uint32_t server_tick,
                                          std::uint32_t snapshot_sequence,
                                          std::uint32_t last_processed_input_sequence) {
        if (!initialized_ || transforms.empty()) {
            return false;
        }

        MessageWriter writer(2048);
        NetworkSnapshotBuilder builder;
        NetworkSnapshotBuildResult build_result;
        if (!writer.Begin(NetMessageType::WorldSnapshot, NextSequence(), 0, server_tick) ||
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

    void NetworkSystem::HandleEvent(NetworkEvent &event) {
        switch (event.type) {
            case NetworkEventType::LobbyCreated:
                session_.BeginHostLobby(event.lobby_id, online_system_.LocalSteamId());
                if (transport_ != nullptr && !transport_->StartHost(0, static_cast<std::uint32_t>(requested_max_players_))) {
                    session_.SetDisconnectReason(NetworkDisconnectReason::TransportError);
                    session_.SetState(NetworkSessionState::Disconnecting);
                }
                break;

            case NetworkEventType::LobbyEntered:
                if (event.lobby_owner_id == online_system_.LocalSteamId()) {
                    session_.BeginHostLobby(event.lobby_id, online_system_.LocalSteamId());
                    break;
                }

                session_.BeginClientLobby(event.lobby_id, event.lobby_owner_id, online_system_.LocalSteamId());
                if (transport_ != nullptr && !transport_->ConnectToHost(event.lobby_owner_id, 0)) {
                    session_.SetDisconnectReason(NetworkDisconnectReason::TransportError);
                    session_.SetState(NetworkSessionState::Disconnecting);
                }
                break;

            case NetworkEventType::LobbyJoinRequested:
                JoinLobbyById(event.lobby_id);
                break;

            case NetworkEventType::LobbyLeft:
                LeaveLobby();
                break;

            case NetworkEventType::PeerConnecting:
                session_.AddOrUpdatePeer(event.peer, event.remote_steam_id, NetworkPeerState::Connecting);
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
                break;

            case NetworkEventType::PeerDisconnected:
                session_.RemovePeer(event.peer);
                session_.SetDisconnectReason(event.disconnect_reason);
                if (session_.Role() == NetworkRole::Client && event.peer == kHostPeerId) {
                    session_.SetState(NetworkSessionState::Disconnecting);
                }
                break;

            case NetworkEventType::AuthAccepted:
                session_.AddOrUpdatePeer(event.peer, event.remote_steam_id, NetworkPeerState::Connected);
                session_.SetState(NetworkSessionState::Connected);
                SendAuthAccepted(event.peer);
                break;

            case NetworkEventType::AuthRejected:
                session_.AddOrUpdatePeer(event.peer, event.remote_steam_id, NetworkPeerState::Closing);
                session_.SetDisconnectReason(event.disconnect_reason);
                SendAuthRejected(event.peer, event.disconnect_reason);
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
        event.payload.assign(payload.begin(), payload.end());

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
                session_.AddOrUpdatePeer(event.peer, event.remote_steam_id, NetworkPeerState::Connected);
                session_.SetState(NetworkSessionState::Connected);
                break;

            case NetMessageType::AuthRejected:
                session_.SetDisconnectReason(NetworkDisconnectReason::AuthenticationFailed);
                session_.SetState(NetworkSessionState::Disconnecting);
                break;

            case NetMessageType::Ping:
                SendEmptyMessage(event.peer, NetMessageType::Pong, SendMode::UnreliableNoDelay);
                break;

            case NetMessageType::Pong:
                break;

            case NetMessageType::InputCommand:
                HandleInputCommandMessage(event);
                break;

            case NetMessageType::WorldSnapshot:
                ++stats_.snapshots_received;
                break;

            case NetMessageType::EntitySpawn:
            case NetMessageType::EntityDespawn:
            case NetMessageType::Disconnect:
                break;
        }
    }

    bool NetworkSystem::SendEmptyMessage(PeerId peer, NetMessageType type, SendMode mode) {
        MessageWriter writer;
        if (!writer.Begin(type, NextSequence(), 0, local_tick_) || !writer.Finalize()) {
            return false;
        }

        return Send(peer, writer.Bytes(), mode);
    }

    bool NetworkSystem::SendHello(PeerId peer, NetMessageType type) {
        MessageWriter writer;
        if (!writer.Begin(type, NextSequence(), 0, local_tick_) ||
            !writer.WriteUInt64(online_system_.LocalSteamId()) ||
            !writer.WriteUInt16(kNetworkProtocolVersion) ||
            !writer.WriteUInt32(kNetworkBuildHash) ||
            !writer.WriteUInt64(handshake_nonce_) ||
            !writer.Finalize()) {
            return false;
        }

        return Send(peer, writer.Bytes(), SendMode::ReliableNoNagle);
    }

    bool NetworkSystem::SendAuthTicket(PeerId peer) {
        if (auth_service_ == nullptr) {
            return false;
        }

        if (auth_service_->LocalTicket().empty() && !auth_service_->CreateLocalTicket()) {
            return false;
        }

        MessageWriter writer;
        if (!writer.Begin(NetMessageType::AuthTicket, NextSequence(), 0, local_tick_) ||
            !writer.WriteUInt64(online_system_.LocalSteamId()) ||
            !writer.WriteSizedBytes(auth_service_->LocalTicket()) ||
            !writer.Finalize()) {
            return false;
        }

        return Send(peer, writer.Bytes(), SendMode::ReliableNoNagle);
    }

    bool NetworkSystem::SendAuthAccepted(PeerId peer) {
        return SendEmptyMessage(peer, NetMessageType::AuthAccepted, SendMode::ReliableNoNagle);
    }

    bool NetworkSystem::SendAuthRejected(PeerId peer, NetworkDisconnectReason reason) {
        MessageWriter writer;
        if (!writer.Begin(NetMessageType::AuthRejected, NextSequence(), 0, local_tick_) ||
            !writer.WriteUInt16(static_cast<std::uint16_t>(reason)) ||
            !writer.Finalize()) {
            return false;
        }

        return Send(peer, writer.Bytes(), SendMode::ReliableNoNagle);
    }

    void NetworkSystem::HandleInputCommandMessage(const NetworkEvent &event) {
        if (session_.Role() != NetworkRole::Host) {
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
        for (std::uint8_t i = 0; i < batch.count; ++i) {
            const PlayerInputCommand &command = batch.commands[i];
            if (command.sequence <= last_sequence) {
                ++stats_.input_commands_duplicated;
                continue;
            }

            last_sequence = command.sequence;
            input_commands_.push_back(QueuedPlayerInputCommand{
                .peer = event.peer,
                .remote_user_id = event.remote_steam_id,
                .command = command,
            });
            ++stats_.input_commands_received;
        }
    }
} // namespace CoreEngine
