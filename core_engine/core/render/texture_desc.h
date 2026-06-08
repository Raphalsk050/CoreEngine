#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/render/render_handle.h"

namespace CoreEngine {
    enum class TextureDimension : std::uint8_t {
        Texture2D,
        Texture2DArray,
        TextureCube,
        TextureCubeArray,
    };

    enum class TextureFormat {
        Auto,
        RGBA8Unorm,
        RGBA8UnormSrgb,
        RGBA16Float,
        RGBA32Float,
        R16Float,
        R32Float,
        RG16Float,
        Depth32Float,
    };

    enum class TextureUsage : std::uint8_t {
        ShaderResource = 1u << 0u,
        RenderTarget = 1u << 1u,
        DepthStencil = 1u << 2u,
    };

    [[nodiscard]] constexpr TextureUsage operator|(TextureUsage lhs, TextureUsage rhs) noexcept {
        return static_cast<TextureUsage>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
    }

    [[nodiscard]] constexpr bool HasTextureUsage(TextureUsage value, TextureUsage usage) noexcept {
        return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(usage)) != 0u;
    };

    enum class TextureLoadState {
        Invalid,
        Pending,
        Ready,
        Failed,
    };

    struct TextureLoadDesc {
        std::string path;
        std::vector<unsigned char> data;
        TextureFormat format = TextureFormat::Auto;
        bool generate_mipmaps = true;
        bool flip_vertically = false;
        bool premultiply_alpha = false;

        [[nodiscard]] bool IsValid() const { return !path.empty() || !data.empty(); }
    };

    /**
     * @brief Describes a GPU-owned texture that may be used by render passes and debug views.
     *
     * Responsibility: keep render-target, depth, array, and cube texture creation
     * backend-independent so PBR passes do not depend on Diligent-specific handles.
     */
    struct TextureDesc {
        std::string debug_name = "Texture";
        int width = 0;
        int height = 0;
        std::uint32_t mip_levels = 1;
        std::uint32_t array_size = 1;
        TextureDimension dimension = TextureDimension::Texture2D;
        TextureFormat format = TextureFormat::RGBA8Unorm;
        TextureUsage usage = TextureUsage::ShaderResource;

        [[nodiscard]] bool IsValid() const {
            if (width <= 0 || height <= 0 || mip_levels == 0u || array_size == 0u) {
                return false;
            }

            if (dimension == TextureDimension::TextureCube && array_size != 6u) {
                return false;
            }

            if (dimension == TextureDimension::TextureCubeArray && (array_size % 6u) != 0u) {
                return false;
            }

            return format != TextureFormat::Auto;
        }
    };

    enum class TextureViewType : std::uint8_t {
        ShaderResource,
        RenderTarget,
        DepthStencil,
    };

    /**
     * @brief Selects a specific texture subresource view for rendering, sampling, or debug display.
     *
     * Responsibility: address mips, array slices, and cube faces without leaking
     * backend texture-view objects to high-level render passes.
     */
    struct TextureViewDesc {
        TextureHandle texture{};
        TextureViewType type = TextureViewType::ShaderResource;
        TextureDimension dimension = TextureDimension::Texture2D;
        std::uint32_t mip_level = 0;
        std::uint32_t mip_count = 1;
        std::uint32_t array_slice = 0;
        std::uint32_t array_slice_count = 1;

        [[nodiscard]] bool IsValid() const {
            return texture.IsValid() && mip_count > 0u && array_slice_count > 0u;
        }
    };
} // namespace CoreEngine
