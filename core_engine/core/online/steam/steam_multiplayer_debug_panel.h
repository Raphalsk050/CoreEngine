#pragma once

#include <memory>

struct ImTextureData;

namespace CoreEngine {
    class IOnlineSystem;

    /**
     * @brief Renders developer controls for Steam multiplayer sessions.
     *
     * Responsibility: keep Steam profile, lobby, transport, and auth diagnostics
     * out of gameplay app code while reusing the engine online module.
     */
    class SteamMultiplayerDebugPanel {
    public:
        SteamMultiplayerDebugPanel() = default;

        ~SteamMultiplayerDebugPanel();

        SteamMultiplayerDebugPanel(const SteamMultiplayerDebugPanel &) = delete;

        SteamMultiplayerDebugPanel &operator=(const SteamMultiplayerDebugPanel &) = delete;

        void Render(IOnlineSystem &online_system);

    private:
        void RenderSteamProfile(IOnlineSystem &online_system);

        void UpdateSteamAvatarTexture(IOnlineSystem &online_system);

        char lobby_id_buffer_[32]{};
        std::unique_ptr<ImTextureData> steam_avatar_texture_;
    };
} // namespace CoreEngine
