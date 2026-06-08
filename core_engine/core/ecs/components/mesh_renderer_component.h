#pragma once

#include "core/render/primitive_topology.h"
#include "core/render/render_handle.h"
#include "core/render/render_mobility.h"

namespace CoreEngine {
    /**
     * @brief Attaches a renderable mesh/material pair to an ECS node.
     *
     * Responsibility: describe visibility, shadow participation, topology, and
     * static-bake eligibility while leaving transform ownership to TransformComponent.
     */
    struct MeshRendererComponent {
        MeshHandle mesh;
        MaterialHandle material;
        bool visible = true;
        bool cast_shadows = true;
        RenderMobility mobility = RenderMobility::Dynamic;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    };

    [[nodiscard]] constexpr bool IsStaticBakeCandidate(const MeshRendererComponent &renderer) noexcept {
        return renderer.visible && renderer.mobility == RenderMobility::Static;
    }
} // namespace CoreEngine
