#pragma once

#include <cstdint>

#include "core/math/math.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    /**
     * @brief Describes a static reflection probe used by PBR environment lighting.
     *
     * Responsibility: author local probe influence independently from the render
     * backend resources generated from the referenced environment map.
     */
    struct ReflectionProbeComponent {
        TextureHandle environment_map{};
        float radius = 10.f;
        Math::Vec3 box_extent{0.f, 0.f, 0.f};
        float intensity = 1.f;
        std::int32_t priority = 0;
        bool use_scene_environment = false;
        bool enabled = true;
    };
} // namespace CoreEngine
