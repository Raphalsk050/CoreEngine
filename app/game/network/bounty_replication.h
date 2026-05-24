#pragma once

namespace CoreEngine {
    class MultiplayerSystem;
}

namespace Game {
    bool RegisterBountyReplicatedComponents(CoreEngine::MultiplayerSystem &multiplayer) noexcept;
}
