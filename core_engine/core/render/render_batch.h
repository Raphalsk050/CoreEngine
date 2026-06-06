#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <tsl/robin_map.h>
#include <vector>
#include "core/math/math.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    struct RenderInstance {
        Math::Mat4 transform{1.f};
    };

    struct RenderBatch {
        MaterialHandle material;
        MeshHandle mesh;

        void AddInstance(const RenderInstance &instance);

        void ClearInstances();

        [[nodiscard]] std::span<const RenderInstance> InlineInstances() const;

        [[nodiscard]] std::span<const RenderInstance> OverflowInstances() const;

        [[nodiscard]] std::size_t InstanceCount() const;

    private:
        static constexpr std::size_t kInlineInstanceCount = 8;

        std::array<RenderInstance, kInlineInstanceCount> inline_instances_{};
        std::size_t inline_instance_count_ = 0;
        std::vector<RenderInstance> overflow_instances_;
    };

    class BatchAccumulator {
    public:
        void Add(MaterialHandle material, MeshHandle mesh, const Math::Mat4 &transform);

        void Reserve(std::size_t expected_instances);

        [[nodiscard]] std::span<const RenderBatch> Batches() const { return {batches_.data(), active_batch_count_}; }

        void Clear();

    private:
        struct BatchKey {
            MaterialHandle material;
            MeshHandle mesh;

            bool operator==(const BatchKey &other) const = default;
        };

        struct BatchKeyHash {
            [[nodiscard]] std::size_t operator()(const BatchKey &key) const noexcept;
        };

        std::vector<RenderBatch> batches_;
        tsl::robin_map<BatchKey, std::size_t, BatchKeyHash> batch_indices_;
        std::size_t active_batch_count_ = 0;
    };
} // namespace CoreEngine
