#pragma once

#include "core/math/math.h"

namespace CoreEngine {
    struct StaticMeshVertex {
        Math::Vec3 position{0.f, 0.f, 0.f};
        Math::Vec3 normal{0.f, 1.f, 0.f};
        Math::Vec3 color{1.f, 1.f, 1.f};
        Math::Vec2 uv{0.f, 0.f};
        Math::Vec4 time{0.016f, 0.f, 0.f, 0.0f};
    };

    using Vertex = StaticMeshVertex;
}
