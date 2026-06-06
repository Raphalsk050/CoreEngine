#include "core/network/network_system.h"

#include "core/log/log.h"
#include "core/network/message_reader.h"
#include "core/network/message_writer.h"
#include "core/online/steam/steam_auth_service.h"
#include "core/online/steam/steam_lobby_service.h"
#include "core/online/steam/steam_online_system.h"
#include "core/online/steam/steam_p2p_transport.h"

namespace CoreEngine {
    namespace {
        constexpr std::uint32_t kNetworkBuildHash = 0;
        constexpr std::uint64_t kHandshakeNonce = 0;
    } // namespace

    NetworkSystem::NetworkSystem(SteamOnlineSystem &online_system) : online_system_(online_system) {}

    NetworkSystem::~NetworkSystem() { Shutdown(); }

    bool NetworkSystem::Initialize() {
        if (initialized_) {
            return true;
        }

        lobby_service_ = std::make_unique<SteamLobbyService>(online_system_);
        transport_ = std::make_unique<SteamP2PTransport>(online_system_);
        auth_service_ = std::make_unique<SteamAuthService>(online_system_);
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
        session_.Reset();
        stats_.Reset();
        initialized_ = false;
    }

    void NetworkSystem::BeginFrame() {
        current_events_.clear();
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

    void NetworkSystem::EndFrame() {}

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

    std::string NetworkSystem::DetailedConnectionStatus(PeerId peer) const {
        return transport_ != nullptr ? transport_->DetailedConnectionStatus(peer) : std::string{};
    }

    void NetworkSystem::HandleEvent(NetworkEvent &event) {
        switch (event.type) {
            case NetworkEventType::LobbyCreated:
                session_.BeginHostLobby(event.lobby_id, online_system_.LocalSteamId());
                if (transport_ != nullptr &&
                    !transport_->StartHost(0, static_cast<std::uint32_t>(requested_max_players_))) {
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

            case NetworkEventType::LobbyJoinRequested: JoinLobbyById(event.lobby_id); break;

            case NetworkEventType::LobbyLeft: LeaveLobby(); break;

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

            case NetworkEventType::PacketReceived: HandleProtocolMessage(event); break;

            case NetworkEventType::None: break;
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

                if (!reader.ReadUInt64(steam_id) || !reader.ReadUInt16(protocol) || !reader.ReadUInt32(build_hash) ||
                    !reader.ReadUInt64(nonce) || protocol != kNetworkProtocolVersion ||
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
            case NetMessageType::InputCommand:
            case NetMessageType::WorldSnapshot:
            case NetMessageType::EntitySpawn:
            case NetMessageType::EntityDespawn:
            case NetMessageType::Disconnect:    break;
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
        if (!writer.Begin(type, NextSequence(), 0, local_tick_) || !writer.WriteUInt64(online_system_.LocalSteamId()) ||
            !writer.WriteUInt16(kNetworkProtocolVersion) || !writer.WriteUInt32(kNetworkBuildHash) ||
            !writer.WriteUInt64(kHandshakeNonce) || !writer.Finalize()) {
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
            !writer.WriteSizedBytes(auth_service_->LocalTicket()) || !writer.Finalize()) {
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
            !writer.WriteUInt16(static_cast<std::uint16_t>(reason)) || !writer.Finalize()) {
            return false;
        }

        return Send(peer, writer.Bytes(), SendMode::ReliableNoNagle);
    }
} // namespace CoreEngine
