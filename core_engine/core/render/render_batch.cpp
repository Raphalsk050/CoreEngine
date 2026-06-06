#include "core/render/render_batch.h"

#include <cstddef>
#include <cstdint>

namespace CoreEngine {
    void RenderBatch::AddInstance(const RenderInstance &instance) {
        if (inline_instance_count_ < inline_instances_.size()) {
            inline_instances_[inline_instance_count_] = instance;
            ++inline_instance_count_;
            return;
        }

        overflow_instances_.push_back(instance);
    }

    void RenderBatch::ClearInstances() {
        inline_instance_count_ = 0;
        overflow_instances_.clear();
    }

    std::span<const RenderInstance> RenderBatch::InlineInstances() const {
        return {inline_instances_.data(), inline_instance_count_};
    }

    std::span<const RenderInstance> RenderBatch::OverflowInstances() const { return overflow_instances_; }

    std::size_t RenderBatch::InstanceCount() const { return inline_instance_count_ + overflow_instances_.size(); }

    std::size_t BatchAccumulator::BatchKeyHash::operator()(const BatchKey &key) const noexcept {
        const auto material_id = static_cast<std::uint64_t>(key.material.id);
        const auto material_generation = static_cast<std::uint64_t>(key.material.generation);
        const auto mesh_id = static_cast<std::uint64_t>(key.mesh.id);
        const auto mesh_generation = static_cast<std::uint64_t>(key.mesh.generation);

        std::uint64_t hash = material_id;
        hash ^= material_generation + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
        hash ^= mesh_id + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
        hash ^= mesh_generation + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
        return static_cast<std::size_t>(hash);
    }

    void BatchAccumulator::Add(MaterialHandle material, MeshHandle mesh, const Math::Mat4 &transform) {
        const BatchKey key{.material = material, .mesh = mesh};
        const auto it = batch_indices_.find(key);

        if (it != batch_indices_.end()) {
            batches_[it.value()].AddInstance(RenderInstance{transform});
            return;
        }

        const std::size_t index = active_batch_count_;
        if (index == batches_.size()) {
            batches_.push_back(RenderBatch{});
        }

        RenderBatch &batch = batches_[index];
        batch.material = material;
        batch.mesh = mesh;
        batch.ClearInstances();
        batch.AddInstance(RenderInstance{transform});

        batch_indices_.emplace(key, index);
        ++active_batch_count_;
    }

    void BatchAccumulator::Reserve(std::size_t expected_instances) {
        if (expected_instances == 0) {
            return;
        }

        batch_indices_.reserve(expected_instances);
    }

    void BatchAccumulator::Clear() {
        for (std::size_t i = 0; i < active_batch_count_; ++i) {
            batches_[i].ClearInstances();
        }

        active_batch_count_ = 0;
        batch_indices_.clear();
    }
} // namespace CoreEngine
