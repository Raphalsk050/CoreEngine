#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "core/online/i_online_system.h"
#include "core/online/steam/steam_config.h"
#include "core/online/steam/steam_online_system.h"

namespace CoreEngine {
    class NetworkSystem;

    /**
     * @brief Owns the online stack exposed to gameplay code.
     *
     * Responsibility: keep Steam lifetime, callback pumping, and network frame
     * flow behind one app-facing module so gameplay code does not wire platform
     * services directly.
     */
    class OnlineSystem final : public IOnlineSystem {
    public:
        explicit OnlineSystem(std::uint32_t steam_app_id = kDefaultSteamAppId) noexcept;

        ~OnlineSystem();

        OnlineSystem(const OnlineSystem &) = delete;

        OnlineSystem &operator=(const OnlineSystem &) = delete;

        bool Initialize();

        void BeginFrame();

        void EndFrame();

        void Shutdown();

        [[nodiscard]] bool IsInitialized() const noexcept {
            return initialized_;
        }

        [[nodiscard]] const OnlineStatus &Status() const noexcept override {
            return status_;
        }

        [[nodiscard]] std::span<const OnlineLobbyMember> LobbyMembers() const noexcept override {
            return lobby_members_;
        }

        [[nodiscard]] OnlineAvatarImage LoadLocalAvatarImage() const override;

        bool CreateFriendsLobby(int max_players) override;

        bool JoinLobbyById(std::uint64_t lobby_id) override;

        bool CreateDirectHost(std::uint16_t port, int max_players) override;

        bool JoinDirect(std::string_view host, std::uint16_t port) override;

        bool OpenSteamOverlay(const char *dialog) override;

        bool OpenInviteOverlay() override;

        void LeaveLobby() override;

        [[nodiscard]] std::string ConnectionDiagnosticsText() const override;

        [[nodiscard]] std::string LocalNetworkAddressText() const override;

        void DumpConnectionStatus() const override;

        [[nodiscard]] NetworkSystem &Network() noexcept;

        [[nodiscard]] const NetworkSystem &Network() const noexcept;

    private:
        void RefreshStatus();

        std::uint32_t steam_app_id_ = kDefaultSteamAppId;
        SteamOnlineSystem steam_;
        std::unique_ptr<NetworkSystem> network_;
        OnlineStatus status_;
        std::vector<OnlineLobbyMember> lobby_members_;
        bool initialized_ = false;
    };
} // namespace CoreEngine
