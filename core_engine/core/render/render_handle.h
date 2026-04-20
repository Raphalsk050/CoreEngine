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
}