#pragma once

#include <cstddef>
#include <span>
#include <vector>
#include <tsl/robin_map.h>
#include "core/math/math.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    struct RenderInstance {
        Math::Mat4 transform{1.f};
    };

    struct RenderBatch {
        MaterialHandle material;
        MeshHandle mesh;
        std::vector<RenderInstance> instances;
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
}
