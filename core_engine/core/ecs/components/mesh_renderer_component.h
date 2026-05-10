#pragma once

#include "GraphicsTypes.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    struct MeshRendererComponent {
        MeshHandle mesh;
        MaterialHandle material;
        bool visible = true;
        bool cast_shadows = true;
        Diligent::PRIMITIVE_TOPOLOGY topology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    };
}
