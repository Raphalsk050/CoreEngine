#include "core/network/replication/network_archetype_registry.h"

namespace CoreEngine {
    bool NetworkArchetypeRegistry::Register(const NetworkArchetypeDesc &desc) noexcept {
        if (desc.archetype_id == 0) {
            return false;
        }

        for (std::size_t i = 0; i < count_; ++i) {
            if (archetypes_[i].archetype_id == desc.archetype_id) {
                archetypes_[i] = desc;
                return true;
            }
        }

        if (count_ >= archetypes_.size()) {
            return false;
        }

        archetypes_[count_++] = desc;
        return true;
    }

    void NetworkArchetypeRegistry::Unregister(NetworkArchetypeId archetype_id) noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            if (archetypes_[i].archetype_id != archetype_id) {
                continue;
            }

            archetypes_[i] = archetypes_[count_ - 1u];
            archetypes_[count_ - 1u] = {};
            --count_;
            return;
        }
    }

    const NetworkArchetypeDesc *NetworkArchetypeRegistry::Find(NetworkArchetypeId archetype_id) const noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            if (archetypes_[i].archetype_id == archetype_id) {
                return &archetypes_[i];
            }
        }

        return nullptr;
    }

    void NetworkArchetypeRegistry::Reset() noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            archetypes_[i] = {};
        }
        count_ = 0;
    }
} // namespace CoreEngine
