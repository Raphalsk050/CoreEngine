#pragma once

#include "core/math/math.h"

namespace CoreEngine {
    struct CameraData {
        Math::Mat4 view       = Math::Mat4(1.f);
        Math::Mat4 projection = Math::Mat4(1.f);
    };
}
