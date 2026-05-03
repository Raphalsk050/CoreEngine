#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/render/shader_binding.h"

namespace CoreEngine {
    struct MaterialDesc {
        std::string vertex_shader_source;
        std::string pixel_shader_source;
        std::vector<uint8_t> properties_data;
        std::vector<ShaderBindingDesc> bindings;
        std::vector<ShaderUniformData> uniforms;
        uint64_t hash = 0;
    };
}
