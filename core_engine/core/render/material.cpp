#include "core/render/material.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <span>
#include <string_view>
#include <utility>

#include "core/log/log.h"
#include "core/render/i_render_context.h"
#include "core/render/builtin_shaders.h"

namespace CoreEngine {
    namespace {
        constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ull;
        constexpr uint64_t kFnvPrime = 1099511628211ull;

        void HashBytes(uint64_t &hash, std::span<const uint8_t> bytes) noexcept {
            for (const uint8_t byte: bytes) {
                hash ^= byte;
                hash *= kFnvPrime;
            }
        }

        void HashValue(uint64_t &hash, uint64_t value) noexcept {
            const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
            HashBytes(hash, std::span<const uint8_t>(bytes, sizeof(value)));
        }

        void HashString(uint64_t &hash, const std::string &value) noexcept {
            const auto *bytes = reinterpret_cast<const uint8_t *>(value.data());
            HashBytes(hash, std::span<const uint8_t>(bytes, value.size()));
            HashValue(hash, static_cast<uint64_t>(value.size()));
        }

        void HashVector(uint64_t &hash, const std::vector<uint8_t> &value) noexcept {
            HashBytes(hash, std::span<const uint8_t>(value.data(), value.size()));
            HashValue(hash, static_cast<uint64_t>(value.size()));
        }

        uint64_t HashDesc(const MaterialDesc &desc) {
            uint64_t h = kFnvOffsetBasis;
            HashString(h, desc.vertex_shader_source);
            HashString(h, desc.pixel_shader_source);
            HashVector(h, desc.properties_data);

            for (const ShaderBindingDesc &binding: desc.bindings) {
                HashString(h, binding.name);
                HashValue(h, static_cast<uint64_t>(binding.type));
                HashValue(h, static_cast<uint64_t>(binding.scope));
                HashValue(h, static_cast<uint64_t>(binding.stages));
                HashValue(h, static_cast<uint64_t>(binding.byte_size));
                HashString(h, binding.sampler_name);
            }
            HashValue(h, static_cast<uint64_t>(desc.bindings.size()));

            for (const ShaderUniformData &uniform: desc.uniforms) {
                HashString(h, uniform.name);
                HashValue(h, static_cast<uint64_t>(uniform.stages));
                HashVector(h, uniform.data);
            }
            HashValue(h, static_cast<uint64_t>(desc.uniforms.size()));

            for (const ShaderTextureData &texture: desc.textures) {
                HashString(h, texture.name);
                HashValue(h, static_cast<uint64_t>(texture.stages));
                HashString(h, texture.sampler_name);
                HashValue(h, static_cast<uint64_t>(texture.texture.id));
                HashValue(h, static_cast<uint64_t>(texture.texture.generation));
            }
            HashValue(h, static_cast<uint64_t>(desc.textures.size()));

            return h;
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

    Material Material::TexturedUnlit(TextureHandle albedo, const TexturedUnlitProps &props) {
        MaterialDesc desc;
        desc.vertex_shader_source = BuiltinShaders::kUnlitVS;
        desc.pixel_shader_source = BuiltinShaders::kTexturedUnlitPS;

        const auto *bytes = reinterpret_cast<const uint8_t *>(&props);
        desc.properties_data.assign(bytes, bytes + sizeof(TexturedUnlitProps));
        desc.uniforms.push_back(MakeShaderUniformData(
            "PerMaterial",
            ShaderStage::Pixel,
            std::span<const uint8_t>(bytes, sizeof(TexturedUnlitProps))));

        if (albedo.IsValid()) {
            constexpr std::string_view kTextureName = "g_Albedo";
            constexpr std::string_view kSamplerName = "g_Albedo_sampler";

            desc.textures.push_back(ShaderTextureData{
                .name = std::string{kTextureName},
                .texture = albedo,
                .stages = ShaderStage::Pixel,
                .sampler_name = std::string{kSamplerName},
            });
            desc.bindings.push_back(ShaderBindingDesc::Texture(
                std::string{kTextureName},
                ShaderBindingScope::Material,
                ShaderStage::Pixel,
                std::string{kSamplerName}));
        }

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

    MaterialBuilder &MaterialBuilder::Vertex(std::string source, bool is_path) {
        std::string src = source;
        if (is_path) {
            std::ifstream in(src);
            std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            src = contents;
        }
        vertex_source_ = src;
        return *this;
    }

    MaterialBuilder &MaterialBuilder::Pixel(std::string source, bool is_path) {
        std::string src = source;
        if (is_path) {
            std::ifstream in(src);
            std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            src = contents;
        }
        pixel_source_ = src;
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

    MaterialBuilder &MaterialBuilder::Texture(std::string name,
                                              TextureHandle texture,
                                              ShaderStage stages,
                                              std::string sampler_name) {
        if (sampler_name.empty()) {
            sampler_name = name + "_sampler";
        }

        textures_.push_back(ShaderTextureData{
            .name = name,
            .texture = texture,
            .stages = stages,
            .sampler_name = sampler_name,
        });

        bindings_.push_back(ShaderBindingDesc::Texture(
            std::move(name),
            ShaderBindingScope::Material,
            stages,
            std::move(sampler_name)));

        return *this;
    }

    Material MaterialBuilder::Build() const {
        MaterialDesc desc;
        desc.vertex_shader_source = vertex_source_;
        desc.pixel_shader_source = pixel_source_;
        desc.properties_data = properties_data_;
        desc.bindings = bindings_;
        desc.uniforms = uniforms_;
        desc.textures = textures_;

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
} // namespace CoreEngine
