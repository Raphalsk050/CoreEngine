#pragma once

#include <cstdint>

namespace CoreEngine {
    /**
     * @brief Classifies renderables for static bake passes and dynamic frame rendering.
     *
     * Responsibility: keep static lighting/probe/shadow bake eligibility explicit
     * without removing dynamic objects from regular per-frame rendering paths.
     */
    enum class RenderMobility : std::uint8_t {
        Static,
        Dynamic,
    };
} // namespace CoreEngine
