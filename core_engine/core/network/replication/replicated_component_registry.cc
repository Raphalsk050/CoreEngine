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

    void ReplicatedComponentRegistry::RegisterDefaultComponents() noexcept {
        const auto add_default = [this](const ReplicatedComponentDesc &desc) noexcept {
            [[maybe_unused]] const bool registered = Register(desc);
        };

        add_default({kNetworkIdentityComponentTypeId, 1, AuthorityPolicy::ServerOnly, ReplicationReliability::ReliableEvent, 0, 0});
        add_default({kNetworkTransformComponentTypeId, 1, AuthorityPolicy::PublicInterpolated, ReplicationReliability::UnreliableSnapshot, 30, static_cast<std::uint32_t>(ReplicatedComponentFlags::Interpolated)});
        add_default({kPlayerMovementStateComponentTypeId, 1, AuthorityPolicy::OwnerPredictedServerAuthoritative, ReplicationReliability::UnreliableSnapshot, 30, static_cast<std::uint32_t>(ReplicatedComponentFlags::Predicted)});
        add_default({kHealthComponentTypeId, 1, AuthorityPolicy::ServerOnly, ReplicationReliability::UnreliableSnapshot, 20, static_cast<std::uint32_t>(ReplicatedComponentFlags::Critical)});
        add_default({kArmorSegmentsComponentTypeId, 1, AuthorityPolicy::ServerOnly, ReplicationReliability::UnreliableSnapshot, 10, 0});
        add_default({kInventoryComponentTypeId, 1, AuthorityPolicy::OwnerOnlyPrivate, ReplicationReliability::ReliableEvent, 0, static_cast<std::uint32_t>(ReplicatedComponentFlags::OwnerOnly)});
        add_default({kEquipmentComponentTypeId, 1, AuthorityPolicy::ServerOnly, ReplicationReliability::ReliableEvent, 0, 0});
        add_default({kBountyBeaconCarrierComponentTypeId, 1, AuthorityPolicy::ServerOnly, ReplicationReliability::ReliableEvent, 0, static_cast<std::uint32_t>(ReplicatedComponentFlags::Critical)});
        add_default({kCaptureStateComponentTypeId, 1, AuthorityPolicy::ServerOnly, ReplicationReliability::ReliableEvent, 0, static_cast<std::uint32_t>(ReplicatedComponentFlags::Critical)});
        add_default({kExtractionStateComponentTypeId, 1, AuthorityPolicy::ServerOnly, ReplicationReliability::ReliableEvent, 0, static_cast<std::uint32_t>(ReplicatedComponentFlags::Critical)});
        add_default({kTargetChainComponentTypeId, 1, AuthorityPolicy::OwnerOnlyPrivate, ReplicationReliability::ReliableEvent, 0, static_cast<std::uint32_t>(ReplicatedComponentFlags::OwnerOnly)});
        add_default({kAIStateComponentTypeId, 1, AuthorityPolicy::ServerOnly, ReplicationReliability::UnreliableSnapshot, 10, 0});
    }
} // namespace CoreEngine
