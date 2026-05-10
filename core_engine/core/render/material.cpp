#include "core/render/material.h"

#include <algorithm>
#include <functional>

#include "core/render/i_render_context.h"
#include "platform/diligent/builtin_shaders.h"

namespace CoreEngine {
    namespace {
        uint64_t HashDesc(const MaterialDesc &desc) {
            size_t h = std::hash<std::string>{}(desc.vertex_shader_source);
            h ^= std::hash<std::string>{}(desc.pixel_shader_source) + 0x9e3779b9u + (h << 6u) + (h >> 2u);

            for (uint8_t byte: desc.properties_data) {
                h ^= std::hash<uint8_t>{}(byte) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
            }

            for (const ShaderBindingDesc &binding: desc.bindings) {
                h ^= std::hash<std::string>{}(binding.name) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
                h ^= static_cast<size_t>(binding.type) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
                h ^= static_cast<size_t>(binding.scope) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
                h ^= static_cast<size_t>(binding.stages) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
                h ^= static_cast<size_t>(binding.byte_size) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
                h ^= std::hash<std::string>{}(binding.sampler_name) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
            }

            for (const ShaderUniformData &uniform: desc.uniforms) {
                h ^= std::hash<std::string>{}(uniform.name) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
                h ^= static_cast<size_t>(uniform.stages) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
                for (uint8_t byte: uniform.data) {
                    h ^= std::hash<uint8_t>{}(byte) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
                }
            }

            return static_cast<uint64_t>(h);
        }
    }

    Material::Material(MaterialDesc desc) : desc_(std::move(desc)) {
    }

    Material Material::Unlit(const UnlitProps &props) {
        MaterialDesc desc;
        desc.vertex_shader_source = BuiltinShaders::kUnlitVS;
        desc.pixel_shader_source = BuiltinShaders::kUnlitPS;

        const auto *bytes = reinterpret_cast<const uint8_t *>(&props);
        desc.properties_data.assign(bytes, bytes + sizeof(UnlitProps));
        desc.uniforms.push_back(MakeShaderUniformData(
            "PerMaterial",
            ShaderStage::Pixel,
            std::span<const uint8_t>(bytes, sizeof(UnlitProps))));
        desc.hash = HashDesc(desc);

        return Material{std::move(desc)};
    }

    Material Material::Custom(const std::string &vs_source,
                              const std::string &ps_source,
                              std::span<const uint8_t> raw_props) {
        MaterialDesc desc;
        desc.vertex_shader_source = vs_source;
        desc.pixel_shader_source = ps_source;
        desc.properties_data.assign(raw_props.begin(), raw_props.end());
        if (!raw_props.empty()) {
            desc.uniforms.push_back(MakeShaderUniformData("PerMaterial", ShaderStage::Pixel, raw_props));
        }
        desc.hash = HashDesc(desc);
        return Material{std::move(desc)};
    }

    MaterialHandle Material::Resolve(IRenderContext &render_context) const {
        return render_context.ResolveMaterial(desc_);
    }

    MaterialBuilder &MaterialBuilder::Vertex(std::string source) {
        vertex_source_ = std::move(source);
        return *this;
    }

    MaterialBuilder &MaterialBuilder::Pixel(std::string source) {
        pixel_source_ = std::move(source);
        return *this;
    }

    MaterialBuilder &MaterialBuilder::Binding(ShaderBindingDesc desc) {
        bindings_.push_back(std::move(desc));
        return *this;
    }

    MaterialBuilder &MaterialBuilder::Uniform(std::string name,
                                              ShaderStage stages,
                                              std::span<const std::uint8_t> raw_data) {
        uniforms_.push_back(MakeShaderUniformData(std::move(name), stages, raw_data));
        return *this;
    }

    Material MaterialBuilder::Build() const {
        MaterialDesc desc;
        desc.vertex_shader_source = vertex_source_;
        desc.pixel_shader_source = pixel_source_;
        desc.properties_data = properties_data_;
        desc.bindings = bindings_;
        desc.uniforms = uniforms_;

        if (!desc.properties_data.empty()) {
            const bool has_legacy_uniform = std::any_of(
                desc.uniforms.begin(),
                desc.uniforms.end(),
                [](const ShaderUniformData &uniform) {
                    return uniform.name == "PerMaterial";
                });

            if (!has_legacy_uniform) {
                desc.uniforms.push_back(MakeShaderUniformData(
                    "PerMaterial",
                    ShaderStage::Pixel,
                    std::span<const uint8_t>(desc.properties_data)));
            }
        }

        desc.hash = HashDesc(desc);
        return Material{std::move(desc)};
    }
}
