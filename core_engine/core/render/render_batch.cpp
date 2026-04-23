#include "core/render/render_batch.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace CoreEngine {
    void BatchAccumulator::Add(MaterialHandle material, MeshHandle mesh, const Math::Mat4 &transform) {
        const BatchKey key{.material = material, .mesh = mesh};
        const auto it = std::find(keys_.begin(), keys_.end(), key);

        if (it != keys_.end()) {
            const std::size_t index = static_cast<std::size_t>(std::distance(keys_.begin(), it));
            batches_[index].instances.push_back({transform});
            return;
        }

        keys_.push_back(key);

        RenderBatch batch;
        batch.material = material;
        batch.mesh = mesh;
        batch.instances.push_back({transform});
        batches_.push_back(std::move(batch));
    }

    void BatchAccumulator::Clear() {
        batches_.clear();
        keys_.clear();
    }
}
