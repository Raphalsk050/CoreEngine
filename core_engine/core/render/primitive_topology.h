#pragma once

namespace CoreEngine {
    /**
     * @brief Engine-owned topology for mesh submission.
     *
     * Responsibility: keep renderable component data independent from concrete
     * graphics backend enum types.
     */
    enum class PrimitiveTopology {
        TriangleList,
        TriangleStrip,
        LineList,
        LineStrip,
        PointList,
    };
} // namespace CoreEngine
