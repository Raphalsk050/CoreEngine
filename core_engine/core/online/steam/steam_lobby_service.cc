#include "core/online/steam/steam_lobby_service.h"

#include <string>

#include "core/log/log.h"
#include "core/online/steam/steam_online_system.h"

namespace CoreEngine {
    SteamLobbyService::SteamLobbyService(SteamOnlineSystem &online_system)
#if CORE_ENGINE_ENABLE_STEAM
        : lobby_join_requested_callback_(this, &SteamLobbyService::OnGameLobbyJoinRequested),
          lobby_chat_update_callback_(this, &SteamLobbyService::OnLobbyChatUpdated),
          online_system_(online_system) {
    }
#else
        : online_system_(online_system) {
    }
#endif

    SteamLobbyService::~SteamLobbyService() {
        LeaveLobby();
    }

    bool SteamLobbyService::CreateFriendsLobby(int max_players) {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamMatchmaking() == nullptr || max_players <= 0) {
            return false;
        }

        const SteamAPICall_t call = SteamMatchmaking()->CreateLobby(k_ELobbyTypeFriendsOnly, max_players);
        if (call == k_uAPICallInvalid) {
            return false;
        }

        lobby_created_call_.Set(call, this, &SteamLobbyService::OnLobbyCreated);
        return true;
#else
        (void) max_players;
        return false;
#endif
    }

    bool SteamLobbyService::JoinLobby(SteamLobbyId lobby_id) {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamMatchmaking() == nullptr || lobby_id == 0) {
            return false;
        }

        const SteamAPICall_t call = SteamMatchmaking()->JoinLobby(CSteamID(lobby_id));
        if (call == k_uAPICallInvalid) {
            return false;
        }

        lobby_entered_call_.Set(call, this, &SteamLobbyService::OnLobbyEntered);
        return true;
#else
        (void) lobby_id;
        return false;
#endif
    }

    bool SteamLobbyService::OpenInviteOverlay() const {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || !online_system_.IsOverlayEnabled() || SteamFriends() == nullptr || current_lobby_id_ == 0) {
            if (online_system_.IsAvailable() && !online_system_.IsOverlayEnabled()) {
                Log::Warn("Steam", "Steam invite overlay requested before the overlay was enabled for this process.");
            }
            return false;
        }

        SteamFriends()->ActivateGameOverlayInviteDialog(CSteamID(current_lobby_id_));
        return true;
#else
        return false;
#endif
    }

    bool SteamLobbyService::InviteUser(SteamId steam_id) const {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamMatchmaking() == nullptr || current_lobby_id_ == 0 || steam_id == 0) {
            return false;
        }

        return SteamMatchmaking()->InviteUserToLobby(CSteamID(current_lobby_id_), CSteamID(steam_id));
#else
        (void) steam_id;
        return false;
#endif
    }

    void SteamLobbyService::LeaveLobby() {
#if CORE_ENGINE_ENABLE_STEAM
        if (online_system_.IsAvailable() && SteamMatchmaking() != nullptr && current_lobby_id_ != 0) {
            SteamMatchmaking()->LeaveLobby(CSteamID(current_lobby_id_));
        }
#endif

        if (current_lobby_id_ != 0) {
            QueueEvent(NetworkEvent{
                .type = NetworkEventType::LobbyLeft,
                .lobby_id = current_lobby_id_,
                .lobby_owner_id = lobby_owner_id_,
            });
        }

        current_lobby_id_ = 0;
        lobby_owner_id_ = 0;
        members_.clear();
    }

    void SteamLobbyService::PollEvents(NetworkEventQueue &out_events) {
        out_events.insert(out_events.end(),
                          std::make_move_iterator(pending_events_.begin()),
                          std::make_move_iterator(pending_events_.end()));
        pending_events_.clear();
    }

    bool SteamLobbyService::SetLobbyData(std::string_view key, std::string_view value) const {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamMatchmaking() == nullptr || current_lobby_id_ == 0) {
            return false;
        }

        const std::string key_string(key);
        const std::string value_string(value);
        return SteamMatchmaking()->SetLobbyData(CSteamID(current_lobby_id_), key_string.c_str(), value_string.c_str());
#else
        (void) key;
        (void) value;
        return false;
#endif
    }

    void SteamLobbyService::QueueEvent(NetworkEvent event) {
        pending_events_.push_back(std::move(event));
    }

    void SteamLobbyService::UpdateMembers() {
        members_.clear();

#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamMatchmaking() == nullptr || current_lobby_id_ == 0) {
            return;
        }

        const CSteamID lobby_id(current_lobby_id_);
        lobby_owner_id_ = SteamMatchmaking()->GetLobbyOwner(lobby_id).ConvertToUint64();

        const int member_count = SteamMatchmaking()->GetNumLobbyMembers(lobby_id);
        members_.reserve(static_cast<std::size_t>(member_count));
        for (int i = 0; i < member_count; ++i) {
            const CSteamID member_id = SteamMatchmaking()->GetLobbyMemberByIndex(lobby_id, i);
            SteamLobbyMember member;
            member.steam_id = member_id.ConvertToUint64();
            if (SteamFriends() != nullptr) {
                member.persona_name = SteamFriends()->GetFriendPersonaName(member_id);
            }
            members_.push_back(std::move(member));
        }
#endif
    }

#if CORE_ENGINE_ENABLE_STEAM
    void SteamLobbyService::OnLobbyCreated(LobbyCreated_t *result, bool io_failure) {
        if (io_failure || result == nullptr || result->m_eResult != k_EResultOK) {
            Log::Warn("Steam", "CreateLobby failed");
            return;
        }

        current_lobby_id_ = result->m_ulSteamIDLobby;
        lobby_owner_id_ = online_system_.LocalSteamId();

        SetLobbyData("protocol", std::to_string(kNetworkProtocolVersion));
        SetLobbyData("host", std::to_string(lobby_owner_id_));
        SetLobbyData("state", "lobby");
        UpdateMembers();

        QueueEvent(NetworkEvent{
            .type = NetworkEventType::LobbyCreated,
            .lobby_id = current_lobby_id_,
            .lobby_owner_id = lobby_owner_id_,
        });
    }

    void SteamLobbyService::OnLobbyEntered(LobbyEnter_t *result, bool io_failure) {
        if (io_failure || result == nullptr || result->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) {
            Log::Warn("Steam", "JoinLobby failed");
            return;
        }

        current_lobby_id_ = result->m_ulSteamIDLobby;
        UpdateMembers();

        QueueEvent(NetworkEvent{
            .type = NetworkEventType::LobbyEntered,
            .lobby_id = current_lobby_id_,
            .lobby_owner_id = lobby_owner_id_,
        });
    }

    void SteamLobbyService::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t *result) {
        if (result == nullptr) {
            return;
        }

        QueueEvent(NetworkEvent{
            .type = NetworkEventType::LobbyJoinRequested,
            .remote_steam_id = result->m_steamIDFriend.ConvertToUint64(),
            .lobby_id = result->m_steamIDLobby.ConvertToUint64(),
        });
    }

    void SteamLobbyService::OnLobbyChatUpdated(LobbyChatUpdate_t *result) {
        if (result == nullptr || result->m_ulSteamIDLobby != current_lobby_id_) {
            return;
        }

        UpdateMembers();
    }
#endif
} // namespace CoreEngine
