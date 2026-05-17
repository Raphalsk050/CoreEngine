#include "core/online/online_system.h"

#include "core/log/log.h"
#include "core/network/network_system.h"

namespace CoreEngine {
    OnlineSystem::OnlineSystem(std::uint32_t steam_app_id) noexcept
        : steam_app_id_(steam_app_id) {
    }

    OnlineSystem::~OnlineSystem() {
        Shutdown();
    }

    bool OnlineSystem::Initialize() {
        if (initialized_) {
            return true;
        }

        if (!steam_.Initialize(steam_app_id_)) {
            Log::Warn("Steam", "Steam online system did not initialize; Steam multiplayer and overlay are disabled.");
        }

        network_ = std::make_unique<NetworkSystem>(steam_);
        if (!network_->Initialize()) {
            Log::Warn("Network", "Network system failed to initialize.");
        }

        initialized_ = true;
        RefreshStatus();
        return true;
    }

    void OnlineSystem::BeginFrame() {
        if (!initialized_) {
            return;
        }

        steam_.PumpCallbacks();
        if (network_ != nullptr) {
            network_->BeginFrame();
        }

        RefreshStatus();
    }

    void OnlineSystem::EndFrame() {
        if (!initialized_ || network_ == nullptr) {
            return;
        }

        network_->EndFrame();
        RefreshStatus();
    }

    void OnlineSystem::Shutdown() {
        if (!initialized_) {
            return;
        }

        if (network_ != nullptr) {
            network_->Shutdown();
            network_.reset();
        }

        steam_.Shutdown();
        initialized_ = false;
        RefreshStatus();
    }

    OnlineAvatarImage OnlineSystem::LoadLocalAvatarImage() const {
        const SteamAvatarImage steam_avatar = steam_.LoadLocalAvatarImage();
        OnlineAvatarImage avatar;
        avatar.user_id = steam_avatar.steam_id;
        avatar.width = steam_avatar.width;
        avatar.height = steam_avatar.height;
        avatar.rgba = steam_avatar.rgba;
        return avatar;
    }

    bool OnlineSystem::CreateFriendsLobby(int max_players) {
        return network_ != nullptr && network_->CreateFriendsLobby(max_players);
    }

    bool OnlineSystem::JoinLobbyById(std::uint64_t lobby_id) {
        return network_ != nullptr && network_->JoinLobbyById(lobby_id);
    }

    bool OnlineSystem::OpenSteamOverlay(const char *dialog) {
        return steam_.OpenOverlay(dialog);
    }

    bool OnlineSystem::OpenInviteOverlay() {
        return network_ != nullptr && network_->OpenInviteOverlay();
    }

    void OnlineSystem::LeaveLobby() {
        if (network_ != nullptr) {
            network_->LeaveLobby();
        }

        RefreshStatus();
    }

    void OnlineSystem::DumpConnectionStatus() const {
        if (network_ != nullptr) {
            network_->DumpConnectionStatus();
        }
    }

    std::string OnlineSystem::ConnectionDiagnosticsText() const {
        return network_ != nullptr ? network_->ConnectionDiagnosticsText() : std::string{};
    }

    NetworkSystem &OnlineSystem::Network() noexcept {
        return *network_;
    }

    const NetworkSystem &OnlineSystem::Network() const noexcept {
        return *network_;
    }

    void OnlineSystem::RefreshStatus() {
        status_.initialized = initialized_;
        status_.steam_available = steam_.IsAvailable();
        status_.steam_overlay_enabled = steam_.IsOverlayEnabled();
        status_.steam_overlay_needs_present = steam_.OverlayNeedsPresent();
        status_.local_user_id = steam_.LocalSteamId();
        status_.local_display_name = steam_.PersonaName();

        lobby_members_.clear();

        if (network_ == nullptr) {
            status_.lobby_id = 0;
            status_.lobby_owner_user_id = 0;
            status_.role = NetworkRole::Offline;
            status_.session_state = NetworkSessionState::Offline;
            status_.last_disconnect_reason = NetworkDisconnectReason::None;
            status_.network_stats.Reset();
            return;
        }

        const NetworkSession &session = network_->Session();
        status_.lobby_id = session.LobbyId();
        status_.lobby_owner_user_id = session.LobbyOwnerSteamId();
        status_.role = session.Role();
        status_.session_state = session.State();
        status_.last_disconnect_reason = session.LastDisconnectReason();
        status_.network_stats = network_->Stats();

        const std::span<const SteamLobbyMember> members = network_->LobbyMembers();
        lobby_members_.reserve(members.size());
        for (const SteamLobbyMember &member: members) {
            lobby_members_.push_back(OnlineLobbyMember{
                .user_id = member.steam_id,
                .display_name = member.persona_name,
            });
        }
    }
} // namespace CoreEngine
