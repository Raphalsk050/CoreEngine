#pragma once

#include "core/render/render_handle.h"
#include "core/render/material.h"

namespace CoreEngine {
    struct MeshRendererComponent {
        MeshHandle mesh;
        Material   material = Material::Unlit();
        bool       visible      = true;
        bool       cast_shadows = true;
    };
}
