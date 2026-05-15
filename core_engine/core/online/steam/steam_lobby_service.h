#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "core/network/network_message.h"
#include "core/online/steam/steam_config.h"
#include "core/online/steam/steam_types.h"

#if CORE_ENGINE_ENABLE_STEAM
#include "steam/steam_api.h"
#endif

namespace CoreEngine {
    class SteamOnlineSystem;

    /**
     * @brief Wraps Steam Lobby creation, invite, join, and metadata callbacks.
     *
     * Responsibility: keep lobby discovery and ownership in the Steam layer while
     * the network session consumes transport-neutral events.
     */
    class SteamLobbyService {
    public:
        explicit SteamLobbyService(SteamOnlineSystem &online_system);

        ~SteamLobbyService();

        bool CreateFriendsLobby(int max_players);

        bool JoinLobby(SteamLobbyId lobby_id);

        bool OpenInviteOverlay() const;

        bool InviteUser(SteamId steam_id) const;

        void LeaveLobby();

        void PollEvents(NetworkEventQueue &out_events);

        bool SetLobbyData(std::string_view key, std::string_view value) const;

        [[nodiscard]] SteamLobbyId CurrentLobbyId() const noexcept {
            return current_lobby_id_;
        }

        [[nodiscard]] SteamId LobbyOwnerId() const noexcept {
            return lobby_owner_id_;
        }

        [[nodiscard]] std::span<const SteamLobbyMember> Members() const noexcept {
            return members_;
        }

    private:
        void QueueEvent(NetworkEvent event);

        void UpdateMembers();

#if CORE_ENGINE_ENABLE_STEAM
        void OnLobbyCreated(LobbyCreated_t *result, bool io_failure);

        void OnLobbyEntered(LobbyEnter_t *result, bool io_failure);

        STEAM_CALLBACK(SteamLobbyService, OnGameLobbyJoinRequested, GameLobbyJoinRequested_t, lobby_join_requested_callback_);

        STEAM_CALLBACK(SteamLobbyService, OnLobbyChatUpdated, LobbyChatUpdate_t, lobby_chat_update_callback_);
#endif

        SteamOnlineSystem &online_system_;
        SteamLobbyId current_lobby_id_ = 0;
        SteamId lobby_owner_id_ = 0;
        std::vector<SteamLobbyMember> members_;
        NetworkEventQueue pending_events_;

#if CORE_ENGINE_ENABLE_STEAM
        CCallResult<SteamLobbyService, LobbyCreated_t> lobby_created_call_;
        CCallResult<SteamLobbyService, LobbyEnter_t> lobby_entered_call_;
#endif
    };
} // namespace CoreEngine
