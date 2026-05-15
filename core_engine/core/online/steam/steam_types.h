#pragma once

#include <cstdint>
#include <string>

namespace CoreEngine {
    using SteamId = std::uint64_t;
    using SteamLobbyId = std::uint64_t;

    enum class SteamLobbyVisibility : std::uint8_t {
        Private,
        FriendsOnly,
        Public,
        Invisible,
    };

    struct SteamLobbyMember {
        SteamId steam_id = 0;
        std::string persona_name;
    };
} // namespace CoreEngine
