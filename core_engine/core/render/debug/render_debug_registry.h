#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/render/frame_buffer.h"
#include "core/render/render_handle.h"
#include "core/render/texture_desc.h"

namespace CoreEngine {
    enum class RenderDebugViewKind : std::uint8_t {
        Color,
        Depth,
        Texture2D,
        Texture2DArraySlice,
        TextureCubeFace,
        ScalarHeatmap,
        Overlay,
        TextStats,
    };

    struct RenderDebugView {
        std::string name;
        RenderDebugViewKind kind = RenderDebugViewKind::Color;
        FrameBufferColorView color_view{};
        FrameBufferDepthView depth_view{};
        TextureViewHandle texture_view{};
        std::uint32_t array_slice = 0;
        std::uint32_t cube_face = 0;
        std::uint32_t mip_level = 0;
        float min_value = 0.f;
        float max_value = 1.f;

        [[nodiscard]] bool IsValid() const { return !name.empty(); }
    };

    struct RenderDebugStats {
        std::uint32_t directional_shadow_draws = 0;
        std::uint32_t point_shadow_draws = 0;
        std::uint32_t shadow_cascade_count = 0;
        std::uint32_t shadowed_point_light_count = 0;
        bool reflection_probe_active = false;
        std::int32_t reflection_probe_priority = 0;
        float reflection_probe_radius = 0.f;
        float reflection_probe_intensity = 0.f;
        float reflection_probe_camera_influence = 0.f;
        std::uint64_t estimated_shadow_bytes = 0;
        std::uint64_t estimated_ibl_bytes = 0;
        float frame_cpu_ms = 0.f;
        float model_upload_cpu_ms = 0.f;
        float frame_setup_cpu_ms = 0.f;
        float shadow_cpu_ms = 0.f;
        float forward_opaque_cpu_ms = 0.f;
        float debug_cpu_ms = 0.f;
        float composite_cpu_ms = 0.f;
        float ui_cpu_ms = 0.f;
        float imgui_cpu_ms = 0.f;
        float present_cpu_ms = 0.f;
        bool ibl_generated_this_frame = false;
    };

    /**
     * @brief Stores inspectable render outputs produced by modular render passes.
     *
     * Responsibility: keep visual debug discovery independent from each feature
     * pass so the renderer can expose a complete list without coupling to PBR
     * implementation details.
     */
    class RenderDebugRegistry final {
    public:
        void BeginFrame();

        void RegisterView(RenderDebugView view);

        [[nodiscard]] std::span<const RenderDebugView> Views() const;

        [[nodiscard]] const RenderDebugView *Find(std::string_view name) const;

        void Select(std::string_view name);

        void ClearSelection();

        [[nodiscard]] const RenderDebugView *SelectedView() const;

        [[nodiscard]] const std::string &SelectedName() const { return selected_name_; }

        [[nodiscard]] RenderDebugStats &Stats() { return stats_; }

        [[nodiscard]] const RenderDebugStats &Stats() const { return stats_; }

    private:
        std::vector<RenderDebugView> views_;
        std::string selected_name_;
        RenderDebugStats stats_{};
    };
} // namespace CoreEngine
