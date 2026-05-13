#pragma once

#include "core/render/render_handle.h"
#include "core/render/primitive_topology.h"

namespace CoreEngine {
    struct MeshRendererComponent {
        MeshHandle mesh;
        MaterialHandle material;
        bool visible = true;
        bool cast_shadows = true;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    };
}
