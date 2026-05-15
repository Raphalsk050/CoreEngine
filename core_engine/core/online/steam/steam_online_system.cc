#include "core/online/steam/steam_online_system.h"

#include <cstddef>

#include "core/log/log.h"

#if CORE_ENGINE_ENABLE_STEAM
#include "steam/isteamnetworkingutils.h"
#include "steam/steam_api.h"
#endif

namespace CoreEngine {
    bool SteamOnlineSystem::Initialize(std::uint32_t app_id) {
#if CORE_ENGINE_ENABLE_STEAM
#if !CENGINE_DEBUG_BUILD
        if (SteamAPI_RestartAppIfNecessary(static_cast<AppId_t>(app_id))) {
            Log::Warn("Steam", "Steam requested restart through the Steam client for AppID {}.", app_id);
            return false;
        }
#else
        (void) app_id;
#endif

        if (!SteamAPI_Init()) {
            Log::Warn("Steam",
                      "SteamAPI_Init failed. Check that Steam is running, steam_appid.txt is next to the executable, steam_api64.dll is present, and the account owns the AppID.");
            return false;
        }

        if (SteamNetworkingUtils() != nullptr) {
            SteamNetworkingUtils()->InitRelayNetworkAccess();
        }

        local_steam_id_ = SteamUser() != nullptr ? SteamUser()->GetSteamID().ConvertToUint64() : 0;
        initialized_ = true;
        Log::Info("Steam", "Steam online system initialized for user {}", local_steam_id_);
        return true;
#else
        (void) app_id;
        Log::Warn("Steam", "Steamworks support was not compiled into this build. Rebuild with CORE_ENGINE_ENABLE_STEAM=ON.");
        initialized_ = false;
        local_steam_id_ = 0;
        return false;
#endif
    }

    void SteamOnlineSystem::PumpCallbacks() {
#if CORE_ENGINE_ENABLE_STEAM
        if (initialized_) {
            SteamAPI_RunCallbacks();
        }
#endif
    }

    void SteamOnlineSystem::Shutdown() {
#if CORE_ENGINE_ENABLE_STEAM
        if (initialized_) {
            SteamAPI_Shutdown();
        }
#endif
        initialized_ = false;
        local_steam_id_ = 0;
    }

    std::uint64_t SteamOnlineSystem::LocalSteamId() const noexcept {
        return local_steam_id_;
    }

    const char *SteamOnlineSystem::PersonaName() const noexcept {
#if CORE_ENGINE_ENABLE_STEAM
        if (initialized_ && SteamFriends() != nullptr) {
            return SteamFriends()->GetPersonaName();
        }
#endif
        return "";
    }

    bool SteamOnlineSystem::IsOverlayEnabled() const noexcept {
#if CORE_ENGINE_ENABLE_STEAM
        return initialized_ && SteamUtils() != nullptr && SteamUtils()->IsOverlayEnabled();
#else
        return false;
#endif
    }

    bool SteamOnlineSystem::OverlayNeedsPresent() const noexcept {
#if CORE_ENGINE_ENABLE_STEAM
        return initialized_ && SteamUtils() != nullptr && SteamUtils()->BOverlayNeedsPresent();
#else
        return false;
#endif
    }

    SteamAvatarImage SteamOnlineSystem::LoadLocalAvatarImage() const {
        SteamAvatarImage image;
        image.steam_id = local_steam_id_;

#if CORE_ENGINE_ENABLE_STEAM
        if (!initialized_ || local_steam_id_ == 0 || SteamFriends() == nullptr || SteamUtils() == nullptr) {
            return image;
        }

        const CSteamID local_user(local_steam_id_);
        const int image_handle = SteamFriends()->GetLargeFriendAvatar(local_user);
        if (image_handle == -1) {
            SteamFriends()->RequestUserInformation(local_user, false);
            return image;
        }

        if (image_handle == 0) {
            return image;
        }

        std::uint32_t width = 0;
        std::uint32_t height = 0;
        if (!SteamUtils()->GetImageSize(image_handle, &width, &height) || width == 0 || height == 0) {
            return image;
        }

        image.image_handle = image_handle;
        image.width = width;
        image.height = height;
        image.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        if (!SteamUtils()->GetImageRGBA(image_handle, image.rgba.data(), static_cast<int>(image.rgba.size()))) {
            image.rgba.clear();
            image.width = 0;
            image.height = 0;
        }
#endif

        return image;
    }

    bool SteamOnlineSystem::OpenOverlay(const char *dialog) const {
#if CORE_ENGINE_ENABLE_STEAM
        if (!initialized_ || SteamFriends() == nullptr || dialog == nullptr) {
            return false;
        }

        if (!IsOverlayEnabled()) {
            Log::Warn("Steam", "Steam overlay is not enabled for this process yet; Shift+Tab will not work until Steam injects the overlay.");
            return false;
        }

        SteamFriends()->ActivateGameOverlay(dialog);
        return true;
#else
        (void) dialog;
        return false;
#endif
    }
} // namespace CoreEngine
