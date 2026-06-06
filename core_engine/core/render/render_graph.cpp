#include "core/render/render_graph.h"

#include <algorithm>
#include <utility>

namespace CoreEngine {
    RenderPassHandle RenderGraph::AddPass(std::unique_ptr<IRenderPass> pass) {
        if (pass == nullptr) {
            return {};
        }

        const RenderPassHandle handle{
                .id = next_pass_id_++,
                .generation = next_generation_++,
        };

        passes_.push_back(PassEntry{
                .handle = handle,
                .desc = pass->Describe(),
                .pass = std::move(pass),
                .insertion_index = next_insertion_index_++,
        });

        SortPasses();
        return handle;
    }

    void RenderGraph::RemovePass(RenderPassHandle handle, IRenderBackend *backend) {
        if (!handle.IsValid()) {
            return;
        }

        for (auto it = passes_.begin(); it != passes_.end();) {
            if (it->handle == handle) {
                if (backend != nullptr && it->pass != nullptr) {
                    it->pass->ReleaseResources(*backend);
                }
                it = passes_.erase(it);
                continue;
            }

            ++it;
        }
    }

    void RenderGraph::Clear(IRenderBackend *backend) {
        for (PassEntry &entry: passes_) {
            if (backend != nullptr && entry.pass != nullptr) {
                entry.pass->ReleaseResources(*backend);
            }
        }
        passes_.clear();
    }

    void RenderGraph::Execute(RenderPassStage stage, RenderPassContext &context) {
        for (PassEntry &entry: passes_) {
            if (entry.desc.stage == stage && entry.pass != nullptr) {
                entry.pass->Execute(context);
            }
        }
    }

    void RenderGraph::SortPasses() {
        std::stable_sort(passes_.begin(), passes_.end(), [](const PassEntry &left, const PassEntry &right) {
            if (left.desc.stage != right.desc.stage) {
                return static_cast<int>(left.desc.stage) < static_cast<int>(right.desc.stage);
            }

            if (left.desc.order != right.desc.order) {
                return left.desc.order < right.desc.order;
            }

            return left.insertion_index < right.insertion_index;
        });
    }
} // namespace CoreEngine
