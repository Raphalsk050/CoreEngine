#include "core/online/steam/steam_auth_service.h"

#include <array>

#include "core/online/steam/steam_online_system.h"

namespace CoreEngine {
    SteamAuthService::SteamAuthService(SteamOnlineSystem &online_system)
#if CORE_ENGINE_ENABLE_STEAM
        : validate_auth_ticket_callback_(this, &SteamAuthService::OnValidateAuthTicketResponse),
          online_system_(online_system) {
    }
#else
        : online_system_(online_system) {
    }
#endif

    SteamAuthService::~SteamAuthService() {
        Shutdown();
    }

    bool SteamAuthService::CreateLocalTicket() {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamUser() == nullptr) {
            return false;
        }

        CancelLocalTicket();

        std::array<std::byte, 2048> ticket_buffer{};
        std::uint32_t ticket_size = 0;
        local_ticket_handle_ = SteamUser()->GetAuthSessionTicket(
            ticket_buffer.data(),
            static_cast<int>(ticket_buffer.size()),
            &ticket_size,
            nullptr);

        if (local_ticket_handle_ == k_HAuthTicketInvalid || ticket_size == 0) {
            local_ticket_.clear();
            return false;
        }

        local_ticket_.assign(ticket_buffer.begin(), ticket_buffer.begin() + ticket_size);
        return true;
#else
        return false;
#endif
    }

    void SteamAuthService::CancelLocalTicket() {
#if CORE_ENGINE_ENABLE_STEAM
        if (online_system_.IsAvailable() && SteamUser() != nullptr && local_ticket_handle_ != k_HAuthTicketInvalid) {
            SteamUser()->CancelAuthTicket(local_ticket_handle_);
        }
        local_ticket_handle_ = k_HAuthTicketInvalid;
#endif
        local_ticket_.clear();
    }

    bool SteamAuthService::BeginAuthSession(PeerId peer, std::uint64_t steam_id, std::span<const std::byte> ticket) {
#if CORE_ENGINE_ENABLE_STEAM
        if (!online_system_.IsAvailable() || SteamUser() == nullptr || peer == kInvalidPeerId ||
            steam_id == 0 || ticket.empty()) {
            return false;
        }

        const EBeginAuthSessionResult result = SteamUser()->BeginAuthSession(
            ticket.data(),
            static_cast<int>(ticket.size()),
            CSteamID(steam_id));

        if (result != k_EBeginAuthSessionResultOK) {
            QueueEvent(NetworkEvent{
                .type = NetworkEventType::AuthRejected,
                .peer = peer,
                .remote_steam_id = steam_id,
                .disconnect_reason = NetworkDisconnectReason::AuthenticationFailed,
            });
            return false;
        }

        pending_auth_[steam_id] = peer;
        return true;
#else
        (void) peer;
        (void) steam_id;
        (void) ticket;
        return false;
#endif
    }

    void SteamAuthService::EndAuthSession(std::uint64_t steam_id) {
#if CORE_ENGINE_ENABLE_STEAM
        if (online_system_.IsAvailable() && SteamUser() != nullptr && steam_id != 0) {
            SteamUser()->EndAuthSession(CSteamID(steam_id));
        }
        pending_auth_.erase(steam_id);
#else
        (void) steam_id;
#endif
    }

    void SteamAuthService::Shutdown() {
        CancelLocalTicket();

#if CORE_ENGINE_ENABLE_STEAM
        if (online_system_.IsAvailable() && SteamUser() != nullptr) {
            for (const auto &[steam_id, peer]: pending_auth_) {
                (void) peer;
                SteamUser()->EndAuthSession(CSteamID(steam_id));
            }
        }
        pending_auth_.clear();
#endif

        pending_events_.clear();
    }

    void SteamAuthService::PollEvents(NetworkEventQueue &out_events) {
        out_events.insert(out_events.end(),
                          std::make_move_iterator(pending_events_.begin()),
                          std::make_move_iterator(pending_events_.end()));
        pending_events_.clear();
    }

    void SteamAuthService::QueueEvent(NetworkEvent event) {
        pending_events_.push_back(std::move(event));
    }

#if CORE_ENGINE_ENABLE_STEAM
    void SteamAuthService::OnValidateAuthTicketResponse(ValidateAuthTicketResponse_t *callback) {
        if (callback == nullptr) {
            return;
        }

        const std::uint64_t steam_id = callback->m_SteamID.ConvertToUint64();
        const auto it = pending_auth_.find(steam_id);
        if (it == pending_auth_.end()) {
            return;
        }

        const bool accepted = callback->m_eAuthSessionResponse == k_EAuthSessionResponseOK;
        QueueEvent(NetworkEvent{
            .type = accepted ? NetworkEventType::AuthAccepted : NetworkEventType::AuthRejected,
            .peer = it->second,
            .remote_steam_id = steam_id,
            .disconnect_reason = accepted ? NetworkDisconnectReason::None : NetworkDisconnectReason::AuthenticationFailed,
        });

        if (!accepted) {
            EndAuthSession(steam_id);
        }
    }
#endif
} // namespace CoreEngine
