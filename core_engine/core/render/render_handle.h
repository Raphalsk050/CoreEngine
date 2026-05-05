#pragma once

#include <cstdint>

namespace CoreEngine {
    template<typename Tag>
    struct GeneratedHandle {
        uint32_t id = 0;
        uint32_t generation = 0;

        [[nodiscard]] bool IsValid() const { return id != 0 && generation != 0; }

        bool operator==(const GeneratedHandle &other) const = default;
    };

    // clang-format off
    struct MeshHandleTag {};
    struct MaterialHandleTag {};
    struct ShaderProgramHandleTag {};
    struct FrameBufferHandleTag {};
    struct RenderPassHandleTag {};
    struct TextureHandleTag {};
    // clang-format on

    using MeshHandle = GeneratedHandle<MeshHandleTag>;
    using TextureHandle = GeneratedHandle<TextureHandleTag>;
    using MaterialHandle = GeneratedHandle<MaterialHandleTag>;
    using ShaderProgramHandle = GeneratedHandle<ShaderProgramHandleTag>;
    using FrameBufferHandle = GeneratedHandle<FrameBufferHandleTag>;
    using RenderPassHandle = GeneratedHandle<RenderPassHandleTag>;
}
