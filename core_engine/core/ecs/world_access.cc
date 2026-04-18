#include "core/ecs/world_access.h"
#include "core/assert/assert.h"

namespace CoreEngine {
    namespace {
        std::atomic<IWorldService *> worldService{nullptr};
    }

    void WorldAccess::Bind(IWorldService &service) {
        worldService.store(&service, std::memory_order_release);
    }

    void WorldAccess::Unbind() {
        worldService.store(nullptr, std::memory_order_release);
    }

    World &WorldAccess::Get() {
        auto *svc = worldService.load(std::memory_order_acquire);
        CENGINE_ASSERT(svc != nullptr, "WorldService not bound");
        return svc->GetWorld();
    }
} // namespace CoreEngine
