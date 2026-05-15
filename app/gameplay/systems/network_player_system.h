#pragma once

#include <cstdint>
#include <vector>

#include "core/ecs/node.h"
#include "core/network/network_peer.h"
#include "core/network/replication/network_identity_component.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    class RenderSystem;
}

namespace Game {
    struct GameplaySystemContext;

    /**
     * @brief Owns runtime player entities that are created from network peers.
     *
     * Responsibility: bridge generic CoreEngine peer/input replication with the
     * Bounty Hunters app's player ECS entities without coupling gameplay to Steam.
     */
    class NetworkPlayerSystem final {
    public:
        void Initialize(CoreEngine::RenderSystem &render_system);

        void Shutdown() noexcept;

        void FixedUpdate(const GameplaySystemContext &context);

        [[nodiscard]] CoreEngine::Node FindPlayerNode(CoreEngine::PeerId peer) const noexcept;

    private:
        struct PlayerRecord {
            CoreEngine::PeerId peer = CoreEngine::kInvalidPeerId;
            std::uint64_t user_id = 0;
            CoreEngine::NetworkEntityId network_id = 0;
            CoreEngine::Node node;
            std::uint32_t last_input_sequence = 0;
        };

        void EnsureHostPeerPlayers(const GameplaySystemContext &context);

        void ApplyHostInputCommands(const GameplaySystemContext &context);

        void EnsureRemoteRenderers(const GameplaySystemContext &context);

        CoreEngine::Node SpawnPeerPlayer(const GameplaySystemContext &context,
                                         CoreEngine::PeerId peer,
                                         std::uint64_t user_id,
                                         std::uint32_t spawn_index);

        [[nodiscard]] PlayerRecord *FindRecord(CoreEngine::PeerId peer) noexcept;

        [[nodiscard]] const PlayerRecord *FindRecord(CoreEngine::PeerId peer) const noexcept;

        CoreEngine::MeshHandle player_mesh_;
        CoreEngine::MaterialHandle remote_material_;
        std::vector<PlayerRecord> players_;
        bool initialized_ = false;
    };
} // namespace Game
