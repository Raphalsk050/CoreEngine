#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/render/render_handle.h"
#include "core/render/shader_binding.h"

namespace CoreEngine {
    struct ShaderTextureData {
        std::string name;
        TextureHandle texture;
        ShaderStage stages = ShaderStage::Pixel;
        std::string sampler_name;

        [[nodiscard]] bool IsValid() const {
            return !name.empty() && texture.IsValid();
        }
    };

    struct MaterialDesc {
        std::string vertex_shader_source;
        std::string pixel_shader_source;
        std::vector<uint8_t> properties_data;
        std::vector<ShaderBindingDesc> bindings;
        std::vector<ShaderUniformData> uniforms;
        std::vector<ShaderTextureData> textures;
        uint64_t hash = 0;
    };
} // namespace CoreEngine
