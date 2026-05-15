#pragma once

#include <cstdint>

#ifndef CORE_ENGINE_ENABLE_STEAM
#define CORE_ENGINE_ENABLE_STEAM 0
#endif

#ifndef CORE_ENGINE_STEAM_APP_ID
#define CORE_ENGINE_STEAM_APP_ID 480
#endif

namespace CoreEngine {
    inline constexpr std::uint32_t kDefaultSteamAppId = CORE_ENGINE_STEAM_APP_ID;
} // namespace CoreEngine
