#include "core/network/replication/replicated_component_registry.h"

namespace CoreEngine {
    bool ReplicatedComponentRegistry::Register(const ReplicatedComponentDesc &desc) noexcept {
        if (desc.component_type_id == 0 || count_ >= components_.size() || Find(desc.component_type_id) != nullptr) {
            return false;
        }

        components_[count_++] = desc;
        return true;
    }

    const ReplicatedComponentDesc *ReplicatedComponentRegistry::Find(
        ReplicatedComponentTypeId component_type_id) const noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            if (components_[i].component_type_id == component_type_id) {
                return &components_[i];
            }
        }

        return nullptr;
    }

    void ReplicatedComponentRegistry::Reset() noexcept {
        components_ = {};
        count_ = 0;
    }

    void ReplicatedComponentRegistry::RegisterDefaultComponents() noexcept {
        const auto add_default = [this](const ReplicatedComponentDesc &desc) noexcept {
            [[maybe_unused]] const bool registered = Register(desc);
        };

        add_default({kNetworkIdentityComponentTypeId, 1, AuthorityPolicy::ServerOnly, ReplicationReliability::ReliableEvent, 0, 0});
        add_default({kNetworkTransformComponentTypeId, 1, AuthorityPolicy::PublicInterpolated, ReplicationReliability::UnreliableSnapshot, 30, static_cast<std::uint32_t>(ReplicatedComponentFlags::Interpolated)});
        add_default({kPlayerMovementStateComponentTypeId, 1, AuthorityPolicy::OwnerPredictedServerAuthoritative, ReplicationReliability::UnreliableSnapshot, 30, static_cast<std::uint32_t>(ReplicatedComponentFlags::Predicted)});
    }
} // namespace CoreEngine
