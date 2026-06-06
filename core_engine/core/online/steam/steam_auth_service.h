#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "core/network/network_message.h"
#include "core/online/steam/steam_config.h"

#if CORE_ENGINE_ENABLE_STEAM
#include "steam/steam_api.h"
#endif

namespace CoreEngine {
    class SteamOnlineSystem;

    /**
     * @brief Owns Steam auth tickets exchanged during session handshake.
     *
     * Responsibility: create local tickets, validate remote tickets, and ensure
     * Steam auth sessions are ended before peers or SteamAPI shut down.
     */
    class SteamAuthService {
    public:
        explicit SteamAuthService(SteamOnlineSystem &online_system);

        ~SteamAuthService();

        bool CreateLocalTicket();

        void CancelLocalTicket();

        bool BeginAuthSession(PeerId peer, std::uint64_t steam_id, std::span<const std::byte> ticket);

        void EndAuthSession(std::uint64_t steam_id);

        void Shutdown();

        void PollEvents(NetworkEventQueue &out_events);

        [[nodiscard]] std::span<const std::byte> LocalTicket() const noexcept { return local_ticket_; }

    private:
        void QueueEvent(NetworkEvent event);

#if CORE_ENGINE_ENABLE_STEAM
        STEAM_CALLBACK(SteamAuthService, OnValidateAuthTicketResponse, ValidateAuthTicketResponse_t,
                       validate_auth_ticket_callback_);
#endif

        SteamOnlineSystem &online_system_;
        std::vector<std::byte> local_ticket_;
        NetworkEventQueue pending_events_;

#if CORE_ENGINE_ENABLE_STEAM
        HAuthTicket local_ticket_handle_ = k_HAuthTicketInvalid;
        std::unordered_map<std::uint64_t, PeerId> pending_auth_;
#endif
    };
} // namespace CoreEngine
