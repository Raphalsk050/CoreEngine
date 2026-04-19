#pragma once

#include <span>
#include <vector>
#include <cstdint>

#include "core/render/render_handle.h"
#include "core/render/vertex.h"
#include "core/render/primitive_type.h"
#include "core/render/material_desc.h"
#include "core/render/camera_data.h"

namespace CoreEngine {
    struct RenderBatch;

    class IRenderContext {
    public:
        virtual ~IRenderContext() = default;

        [[nodiscard]] virtual MeshHandle GetOrCreatePrimitive(PrimitiveType type) = 0;

        [[nodiscard]] virtual MeshHandle CreateMesh(std::span<const Vertex> vertices,
                                                    std::span<const uint16_t> indices) = 0;

        [[nodiscard]] virtual MaterialHandle ResolveMaterial(const MaterialDesc &desc) = 0;

        virtual void SetCamera(const CameraData &camera) = 0;

        virtual void SubmitBatch(const RenderBatch &batch) = 0;
    };
}
