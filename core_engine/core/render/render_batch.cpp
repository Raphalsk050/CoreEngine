#include "core/render/render_batch.h"

#include <cstddef>
#include <cstdint>

namespace CoreEngine {
    std::size_t BatchAccumulator::BatchKeyHash::operator()(const BatchKey &key) const noexcept {
        const auto material_id = static_cast<std::uint64_t>(key.material.id);
        const auto mesh_id = static_cast<std::uint64_t>(key.mesh.id);
        const auto mesh_generation = static_cast<std::uint64_t>(key.mesh.generation);

        std::uint64_t hash = material_id;
        hash ^= mesh_id + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
        hash ^= mesh_generation + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
        return static_cast<std::size_t>(hash);
    }

    void BatchAccumulator::Add(MaterialHandle material, MeshHandle mesh, const Math::Mat4 &transform) {
        const BatchKey key{.material = material, .mesh = mesh};
        const auto it = batch_indices_.find(key);

        if (it != batch_indices_.end()) {
            batches_[it.value()].instances.push_back({transform});
            return;
        }

        const std::size_t index = active_batch_count_;
        if (index == batches_.size()) {
            batches_.push_back(RenderBatch{});
        }

        RenderBatch &batch = batches_[index];
        batch.material = material;
        batch.mesh = mesh;
        batch.instances.clear();
        batch.instances.push_back({transform});

        batch_indices_.emplace(key, index);
        ++active_batch_count_;
    }

    void BatchAccumulator::Reserve(std::size_t expected_instances) {
        if (expected_instances == 0) {
            return;
        }

        batches_.reserve(expected_instances);
        batch_indices_.reserve(expected_instances);
    }

    void BatchAccumulator::Clear() {
        for (std::size_t i = 0; i < active_batch_count_; ++i) {
            batches_[i].instances.clear();
        }

        active_batch_count_ = 0;
        batch_indices_.clear();
    }
}
