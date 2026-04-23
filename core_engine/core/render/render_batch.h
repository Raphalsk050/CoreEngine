#pragma once

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
        std::vector<RenderInstance> instances;
    };

    class BatchAccumulator {
    public:
        void Add(MaterialHandle material, MeshHandle mesh, const Math::Mat4 &transform);

        [[nodiscard]] const std::vector<RenderBatch> &Batches() const { return batches_; }

        void Clear();

    private:
        struct BatchKey {
            MaterialHandle material;
            MeshHandle mesh;

            bool operator==(const BatchKey &other) const = default;
        };

        std::vector<RenderBatch> batches_;
        std::vector<BatchKey> keys_;
    };
}