#include "core/render/debug/render_debug_registry.h"

#include <algorithm>
#include <utility>

namespace CoreEngine {
    void RenderDebugRegistry::BeginFrame() {
        views_.clear();
        stats_ = {};
    }

    void RenderDebugRegistry::RegisterView(RenderDebugView view) {
        if (!view.IsValid()) {
            return;
        }

        const auto existing = std::ranges::find_if(views_, [&view](const RenderDebugView &registered) {
            return registered.name == view.name;
        });
        if (existing != views_.end()) {
            *existing = std::move(view);
            return;
        }

        views_.push_back(std::move(view));
    }

    std::span<const RenderDebugView> RenderDebugRegistry::Views() const { return views_; }

    const RenderDebugView *RenderDebugRegistry::Find(std::string_view name) const {
        const auto it = std::ranges::find_if(views_, [name](const RenderDebugView &view) {
            return view.name == name;
        });
        return it == views_.end() ? nullptr : &*it;
    }

    void RenderDebugRegistry::Select(std::string_view name) { selected_name_ = std::string{name}; }

    void RenderDebugRegistry::ClearSelection() { selected_name_.clear(); }

    const RenderDebugView *RenderDebugRegistry::SelectedView() const {
        return selected_name_.empty() ? nullptr : Find(selected_name_);
    }
} // namespace CoreEngine
