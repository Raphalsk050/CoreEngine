#pragma once

#include <cstdint>

namespace CoreEngine {
    struct MeshHandle {
        uint32_t id = 0;
        uint32_t generation = 0;

        [[nodiscard]] bool IsValid() const { return id != 0 && generation != 0; }
        bool operator==(const MeshHandle &other) const = default;
    };

    struct MaterialHandle {
        uint32_t id = 0;
        [[nodiscard]] bool IsValid() const { return id != 0; }
        bool operator==(const MaterialHandle &other) const = default;
    };

    struct ShaderProgramHandle {
        uint32_t id = 0;
        uint32_t generation = 0;

        [[nodiscard]] bool IsValid() const { return id != 0 && generation != 0; }
        bool operator==(const ShaderProgramHandle &other) const = default;
    };

    struct FrameBufferHandle {
        uint32_t id = 0;
        uint32_t generation = 0;

        [[nodiscard]] bool IsValid() const { return id != 0 && generation != 0; }
        bool operator==(const FrameBufferHandle &other) const = default;
    };

    struct RenderPassHandle {
        uint32_t id = 0;
        uint32_t generation = 0;

        [[nodiscard]] bool IsValid() const { return id != 0 && generation != 0; }
        bool operator==(const RenderPassHandle &other) const = default;
    };
}
