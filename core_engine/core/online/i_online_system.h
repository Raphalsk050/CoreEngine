#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/network/network_session.h"
#include "core/network/network_stats.h"

namespace CoreEngine {
    struct OnlineAvatarImage {
        std::uint64_t user_id = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> rgba;

        [[nodiscard]] bool IsValid() const noexcept {
            return user_id != 0 && width > 0 && height > 0 && !rgba.empty();
        }
    };

    struct OnlineLobbyMember {
        std::uint64_t user_id = 0;
        std::string display_name;
    };

    struct OnlineStatus {
        bool initialized = false;
        bool steam_available = false;
        bool steam_overlay_enabled = false;
        bool steam_overlay_needs_present = false;
        std::uint64_t local_user_id = 0;
        std::string local_display_name;
        std::uint64_t lobby_id = 0;
        std::uint64_t lobby_owner_user_id = 0;
        NetworkRole role = NetworkRole::Offline;
        NetworkSessionState session_state = NetworkSessionState::Offline;
        NetworkDisconnectReason last_disconnect_reason = NetworkDisconnectReason::None;
        NetworkStats network_stats;
    };

    /**
     * @brief App-facing interface for online identity, lobby, and network session state.
     *
     * Responsibility: expose stable, gameplay-friendly online state and commands
     * without leaking Steamworks transport or authentication objects to apps.
     */
    class IOnlineSystem {
    public:
        virtual ~IOnlineSystem() = default;

        [[nodiscard]] virtual const OnlineStatus &Status() const noexcept = 0;

        [[nodiscard]] virtual std::span<const OnlineLobbyMember> LobbyMembers() const noexcept = 0;

        [[nodiscard]] virtual OnlineAvatarImage LoadLocalAvatarImage() const = 0;

        virtual bool CreateFriendsLobby(int max_players) = 0;

        virtual bool JoinLobbyById(std::uint64_t lobby_id) = 0;

        virtual bool OpenSteamOverlay(const char *dialog) = 0;

        virtual bool OpenInviteOverlay() = 0;

        virtual void LeaveLobby() = 0;

        virtual void DumpConnectionStatus() const = 0;
    };
} // namespace CoreEngine
