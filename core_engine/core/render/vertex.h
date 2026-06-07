#pragma once

#include "core/math/math.h"

namespace CoreEngine {
    struct StaticMeshVertex {
        Math::Vec3 position{0.f, 0.f, 0.f};
        Math::Vec3 normal{0.f, 1.f, 0.f};
        Math::Vec3 color{1.f, 1.f, 1.f};
        Math::Vec2 uv{0.f, 0.f};
        // PBR materials use xyz as tangent and w as bitangent handedness.
        Math::Vec4 custom0{0.f, 0.f, 0.f, 0.f};
    };

    using Vertex = StaticMeshVertex;
} // namespace CoreEngine
