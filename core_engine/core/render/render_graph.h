#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/render/render_handle.h"
#include "core/render/render_pass.h"

namespace CoreEngine {
    class RenderGraph final {
    public:
        [[nodiscard]] RenderPassHandle AddPass(std::unique_ptr<IRenderPass> pass);

        void RemovePass(RenderPassHandle handle, IRenderBackend *backend = nullptr);

        void Clear(IRenderBackend *backend = nullptr);

        void Execute(RenderPassStage stage, RenderPassContext &context);

        [[nodiscard]] bool HasPasses() const { return !passes_.empty(); }

    private:
        struct PassEntry {
            RenderPassHandle handle;
            RenderPassDesc desc;
            std::unique_ptr<IRenderPass> pass;
            uint32_t insertion_index = 0;
        };

        void SortPasses();

        std::vector<PassEntry> passes_;
        uint32_t next_pass_id_ = 1;
        uint32_t next_generation_ = 1;
        uint32_t next_insertion_index_ = 0;
    };
} // namespace CoreEngine
