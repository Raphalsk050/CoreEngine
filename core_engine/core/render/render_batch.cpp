#include "core/render/render_batch.h"

#include <algorithm>

namespace CoreEngine {
    void BatchAccumulator::Add(MaterialHandle material, MeshHandle mesh, const glm::mat4 &transform) {
        const uint64_t key = MakeKey(material, mesh);
        const auto it = std::find(keys_.begin(), keys_.end(), key);

        if (it != keys_.end()) {
            const size_t index = static_cast<size_t>(std::distance(keys_.begin(), it));
            batches_[index].instances.push_back({transform});
        } else {
            keys_.push_back(key);
            RenderBatch batch;
            batch.material = material;
            batch.mesh     = mesh;
            batch.instances.push_back({transform});
            batches_.push_back(std::move(batch));
        }
    }

    void BatchAccumulator::Clear() {
        batches_.clear();
        keys_.clear();
    }
}
