#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/network/replication/replicated_state_types.h"

namespace CoreEngine {
    struct ReplicatedComponentDesc {
        ReplicatedComponentTypeId component_type_id = 0;
        std::uint16_t serialization_version = 1;
        AuthorityPolicy authority = AuthorityPolicy::ServerOnly;
        ReplicationReliability reliability = ReplicationReliability::UnreliableSnapshot;
        std::uint8_t max_send_rate = 20;
        std::uint32_t flags = 0;
    };

    /**
     * @brief Stores metadata for component replication policy.
     *
     * Responsibility: keep component authority and send-rate declarations in a
     * stable registry that snapshot builders and gameplay systems can query.
     */
    class ReplicatedComponentRegistry {
    public:
        [[nodiscard]] bool Register(const ReplicatedComponentDesc &desc) noexcept;

        template<typename Component>
        [[nodiscard]] bool RegisterComponent(const ReplicatedComponentDesc &desc) noexcept {
            return Register(desc);
        }

        [[nodiscard]] const ReplicatedComponentDesc *Find(ReplicatedComponentTypeId component_type_id) const noexcept;

        [[nodiscard]] std::span<const ReplicatedComponentDesc> Components() const noexcept {
            return std::span<const ReplicatedComponentDesc>{components_.data(), count_};
        }

        void RegisterDefaultComponents() noexcept;

    private:
        static constexpr std::size_t kMaxReplicatedComponents = 64;

        std::array<ReplicatedComponentDesc, kMaxReplicatedComponents> components_{};
        std::size_t count_ = 0;
    };
} // namespace CoreEngine
