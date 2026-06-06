#pragma once

#include "core/render/primitive_topology.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    struct MeshRendererComponent {
        MeshHandle mesh;
        MaterialHandle material;
        bool visible = true;
        bool cast_shadows = true;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    };
} // namespace CoreEngine
