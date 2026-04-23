#include "core/render/material.h"

#include <functional>

#include "core/render/i_render_context.h"
#include "platform/diligent/builtin_shaders.h"

namespace CoreEngine {
    namespace {
        uint64_t HashDesc(const MaterialDesc &desc) {
            size_t h = std::hash<std::string>{}(desc.vertex_shader_source);
            h ^= std::hash<std::string>{}(desc.pixel_shader_source) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
            for (uint8_t byte : desc.properties_data) {
                h ^= std::hash<uint8_t>{}(byte) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
            }
            return static_cast<uint64_t>(h);
        }
    }

    Material::Material(MaterialDesc desc) : desc_(std::move(desc)) {}

    Material Material::Unlit(const UnlitProps &props) {
        MaterialDesc desc;
        desc.vertex_shader_source = BuiltinShaders::kUnlitVS;
        desc.pixel_shader_source  = BuiltinShaders::kUnlitPS;

        const auto *bytes = reinterpret_cast<const uint8_t *>(&props);
        desc.properties_data.assign(bytes, bytes + sizeof(UnlitProps));
        desc.hash = HashDesc(desc);

        return Material{std::move(desc)};
    }

    Material Material::Custom(const std::string &vs_source,
                              const std::string &ps_source,
                              std::span<const uint8_t> raw_props) {
        MaterialDesc desc;
        desc.vertex_shader_source = vs_source;
        desc.pixel_shader_source  = ps_source;
        desc.properties_data.assign(raw_props.begin(), raw_props.end());
        desc.hash = HashDesc(desc);
        return Material{std::move(desc)};
    }

    MaterialHandle Material::Resolve(IRenderContext &ctx) const {
        return ctx.ResolveMaterial(desc_);
    }

    MaterialBuilder &MaterialBuilder::Vertex(std::string source) {
        vertex_source_ = std::move(source);
        return *this;
    }

    MaterialBuilder &MaterialBuilder::Pixel(std::string source) {
        pixel_source_ = std::move(source);
        return *this;
    }

    Material MaterialBuilder::Build() const {
        return Material::Custom(vertex_source_, pixel_source_,
                                std::span<const uint8_t>(properties_data_));
    }
}
