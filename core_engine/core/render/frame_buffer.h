#pragma once

namespace CoreEngine {
    struct FrameBufferDesc {
        int width = 0;
        int height = 0;
        bool sample_color = true;

        [[nodiscard]] bool IsValid() const {
            return width > 0 && height > 0;
        }
    };
} // namespace CoreEngine
