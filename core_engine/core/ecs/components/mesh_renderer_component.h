#pragma once

#include "core/render/render_handle.h"

namespace CoreEngine {
    struct MeshRendererComponent {
        MeshHandle mesh;
        MaterialHandle material;
        bool visible = true;
        bool cast_shadows = true;
    };
}