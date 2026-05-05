#pragma once

#include <string>

namespace CoreEngine {
    enum class TextureFormat {
        Auto,
        RGBA8Unorm,
        RGBA8UnormSrgb,
    };

    enum class TextureLoadState {
        Invalid,
        Pending,
        Ready,
        Failed,
    };

    struct TextureLoadDesc {
        std::string path;
        TextureFormat format = TextureFormat::Auto;
        bool generate_mipmaps = true;
        bool flip_vertically = false;
        bool premultiply_alpha = false;

        [[nodiscard]] bool IsValid() const {
            return !path.empty();
        }
    };
} // namespace CoreEngine
