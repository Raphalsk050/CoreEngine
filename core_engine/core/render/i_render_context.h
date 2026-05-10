#pragma once

#include "core/render/material_desc.h"
#include "core/render/mesh_desc.h"
#include "core/render/primitive_type.h"
#include "core/render/render_handle.h"
#include "core/render/shader_binding.h"

namespace CoreEngine {
    class IRenderContext {
    public:
        virtual ~IRenderContext() = default;

        [[nodiscard]] virtual MeshHandle GetOrCreatePrimitive(PrimitiveType type) = 0;

        [[nodiscard]] virtual MeshHandle CreateMesh(const MeshDesc &desc) = 0;

        [[nodiscard]] virtual MaterialHandle ResolveMaterial(const MaterialDesc &desc) = 0;

        [[nodiscard]] virtual ShaderProgramHandle CreateShaderProgram(const ShaderProgramDesc &desc) = 0;

        virtual void DestroyShaderProgram(ShaderProgramHandle handle) = 0;
    };
}
