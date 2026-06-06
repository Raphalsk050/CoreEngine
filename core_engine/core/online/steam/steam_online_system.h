#pragma once

#include <cstdint>
#include <vector>

#include "core/online/steam/steam_config.h"

namespace CoreEngine {
    struct SteamAvatarImage {
        std::uint64_t steam_id = 0;
        int image_handle = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> rgba;

        [[nodiscard]] bool IsValid() const noexcept {
            return steam_id != 0 && width > 0 && height > 0 && !rgba.empty();
        }
    };

    /**
     * @brief Owns the Steamworks process-level API lifetime.
     *
     * Responsibility: initialize SteamAPI, pump callbacks once per frame, expose
     * local identity, and shut the API down in deterministic runtime order.
     */
    class SteamOnlineSystem {
    public:
        bool Initialize(std::uint32_t app_id = kDefaultSteamAppId);

        void PumpCallbacks();

        void Shutdown();

        [[nodiscard]] bool IsAvailable() const noexcept { return initialized_; }

        [[nodiscard]] std::uint64_t LocalSteamId() const noexcept;

        [[nodiscard]] const char *PersonaName() const noexcept;

        [[nodiscard]] bool IsOverlayEnabled() const noexcept;

        [[nodiscard]] bool OverlayNeedsPresent() const noexcept;

        [[nodiscard]] SteamAvatarImage LoadLocalAvatarImage() const;

        bool OpenOverlay(const char *dialog) const;

    private:
        bool initialized_ = false;
        std::uint64_t local_steam_id_ = 0;
    };
} // namespace CoreEngine
