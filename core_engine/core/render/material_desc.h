#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace CoreEngine {
    struct MaterialDesc {
        std::string vertex_shader_source;
        std::string pixel_shader_source;
        std::vector<uint8_t> properties_data;
        uint64_t hash = 0;
    };
}
