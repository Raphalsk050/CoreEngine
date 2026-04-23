#pragma once

#include <span>
#include <vector>

#include "core/math/math.h"
#include "core/render/render_handle.h"
#include "core/render/material_desc.h"

namespace CoreEngine {
    class IRenderContext;

    struct UnlitProps {
        alignas(16) Math::Vec4 color{1.f, 1.f, 1.f, 1.f};
    };

    class Material {
    public:
        [[nodiscard]] static Material Unlit(const UnlitProps &props = {});

        [[nodiscard]] static Material Custom(const std::string &vs_source,
                                             const std::string &ps_source,
                                             std::span<const uint8_t> raw_props);

        template<typename T>
        [[nodiscard]] static Material Custom(const std::string &vs_source,
                                             const std::string &ps_source,
                                             const T &props) {
            return Custom(vs_source, ps_source,
                          std::span<const uint8_t>(
                              reinterpret_cast<const uint8_t *>(&props),
                              sizeof(T)));
        }

        [[nodiscard]] MaterialHandle Resolve(IRenderContext &ctx) const;

    private:
        explicit Material(MaterialDesc desc);
        MaterialDesc desc_;
    };

    class MaterialBuilder {
    public:
        MaterialBuilder &Vertex(std::string source);
        MaterialBuilder &Pixel(std::string source);

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
    };
}
