#pragma once

namespace CoreEngine {
    struct DepthVisualizationDesc {
        float scale = 1.0f;
        float bias = 0.0f;
        float exponent = 1.0f;
        bool invert = false;
    };
} // namespace CoreEngine
