#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/ecs/node.h"
#include "core/network/network_peer.h"
#include "core/network/replication/network_identity_component.h"

namespace CoreEngine {
    class World;

    struct NetworkArchetypeSpawnContext {
        World &world;
        Node node;
        NetworkEntityId network_id = 0;
        PeerId owner_peer = kInvalidPeerId;
        NetworkArchetypeId archetype_id = 0;
        NetworkPresentationId presentation_id = 0;
        bool local_authority = false;
    };

    using NetworkArchetypeSpawnFn = void (*)(NetworkArchetypeSpawnContext &context, void *user_data);

    struct NetworkArchetypeDesc {
        NetworkArchetypeId archetype_id = 0;
        const char *debug_name = "RemoteNetworkEntity";
        NetworkArchetypeSpawnFn initialize_remote_entity = nullptr;
        void *user_data = nullptr;
    };

    /**
     * @brief Stores app-provided replicated entity archetype factories.
     *
     * Responsibility: let the engine own remote spawn timing while the game
     * supplies archetype-specific components and presentation setup.
     */
    class NetworkArchetypeRegistry {
    public:
        [[nodiscard]] bool Register(const NetworkArchetypeDesc &desc) noexcept;

        void Unregister(NetworkArchetypeId archetype_id) noexcept;

        [[nodiscard]] const NetworkArchetypeDesc *Find(NetworkArchetypeId archetype_id) const noexcept;

        void Reset() noexcept;

    private:
        static constexpr std::size_t kMaxArchetypes = 64;

        std::array<NetworkArchetypeDesc, kMaxArchetypes> archetypes_{};
        std::size_t count_ = 0;
    };
} // namespace CoreEngine
