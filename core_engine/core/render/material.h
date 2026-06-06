#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/math/math.h"
#include "core/render/material_desc.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    class IRenderContext;

    struct UnlitProps {
        alignas(16) Math::Vec4 color{1.f, 1.f, 1.f, 1.f};
    };

    struct TexturedUnlitProps {
        alignas(16) Math::Vec4 color{1.f, 1.f, 1.f, 1.f};
    };

    class Material {
    public:
        [[nodiscard]] static Material Unlit(const UnlitProps &props = {});

        [[nodiscard]] static Material TexturedUnlit(TextureHandle albedo, const TexturedUnlitProps &props = {});

        [[nodiscard]] static Material Custom(const std::string &vs_source, const std::string &ps_source,
                                             std::span<const uint8_t> raw_props);

        template<typename T>
        [[nodiscard]] static Material Custom(const std::string &vs_source, const std::string &ps_source,
                                             const T &props) {
            return Custom(vs_source, ps_source,
                          std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(&props), sizeof(T)));
        }

        [[nodiscard]] MaterialHandle Resolve(IRenderContext &render_context) const;

    private:
        friend class MaterialBuilder;

        explicit Material(MaterialDesc desc);

        MaterialDesc desc_;
    };

    class MaterialBuilder {
    public:
        MaterialBuilder &Vertex(std::string source, bool is_path = false);

        MaterialBuilder &Pixel(std::string source, bool is_path = false);

        MaterialBuilder &Binding(ShaderBindingDesc desc);

        MaterialBuilder &Uniform(std::string name, ShaderStage stages, std::span<const std::uint8_t> raw_data);

        MaterialBuilder &Texture(std::string name, TextureHandle texture, ShaderStage stages = ShaderStage::Pixel,
                                 std::string sampler_name = {});

        template<typename T>
        MaterialBuilder &Uniform(std::string name, const T &data, ShaderStage stages = ShaderStage::Pixel) {
            return Uniform(std::move(name), stages,
                           std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(&data), sizeof(T)));
        }

        template<typename T>
        MaterialBuilder &Properties(const T &props) {
            const auto *bytes = reinterpret_cast<const uint8_t *>(&props);
            properties_data_.assign(bytes, bytes + sizeof(T));
            return *this;
        }

        [[nodiscard]] Material Build() const;

    private:
        std::string vertex_source_;
        std::string pixel_source_;
        std::vector<uint8_t> properties_data_;
        std::vector<ShaderBindingDesc> bindings_;
        std::vector<ShaderUniformData> uniforms_;
        std::vector<ShaderTextureData> textures_;
    };
} // namespace CoreEngine
