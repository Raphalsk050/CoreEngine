#pragma once

#include <span>
#include <vector>

#include "core/math/math.h"

namespace CoreEngine {
    enum class DebugDrawShapeType {
        Line,
        Box,
        Sphere,
    };

    struct DebugDrawStyle {
        Math::Vec4 color{0.0f, 1.0f, 0.0f, 1.0f};
        float duration_seconds = 0.0f;
        bool depth_test = true;
    };

    struct DebugDrawCommand {
        DebugDrawShapeType shape = DebugDrawShapeType::Line;
        Math::Vec3 start{};
        Math::Vec3 end{};
        Math::Vec3 center{};
        Math::Vec3 half_extents{};
        Math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        float radius = 0.0f;
        DebugDrawStyle style{};
        float remaining_seconds = 0.0f;
    };

    struct DebugTraceResult {
        bool hit = false;
        Math::Vec3 point{};
        Math::Vec3 normal{0.0f, 1.0f, 0.0f};
    };

    /**
     * @brief Owns transient debug geometry requested by engine and gameplay systems.
     *
     * Responsibility: provide a renderer-facing command buffer for line, box,
     * sphere, and trace visualization without coupling gameplay systems to a
     * concrete render backend.
     */
    class DebugDrawSystem final {
    public:
        void BeginFrame(float delta_seconds);

        void Clear() noexcept;

        void DrawLine(const Math::Vec3 &start,
                      const Math::Vec3 &end,
                      const DebugDrawStyle &style = {});

        void DrawBox(const Math::Vec3 &center,
                     const Math::Vec3 &half_extents,
                     const Math::Quat &rotation = Math::Quat{1.0f, 0.0f, 0.0f, 0.0f},
                     const DebugDrawStyle &style = {});

        void DrawSphere(const Math::Vec3 &center,
                        float radius,
                        const DebugDrawStyle &style = {});

        void DrawLineTrace(const Math::Vec3 &start,
                           const Math::Vec3 &end,
                           const DebugTraceResult &result,
                           const DebugDrawStyle &style = {});

        void DrawBoxTrace(const Math::Vec3 &start,
                          const Math::Vec3 &end,
                          const Math::Vec3 &half_extents,
                          const Math::Quat &rotation,
                          const DebugTraceResult &result,
                          const DebugDrawStyle &style = {});

        void DrawSphereTrace(const Math::Vec3 &start,
                             const Math::Vec3 &end,
                             float radius,
                             const DebugTraceResult &result,
                             const DebugDrawStyle &style = {});

        [[nodiscard]] std::span<const DebugDrawCommand> Commands() const noexcept {
            return commands_;
        }

    private:
        void Push(DebugDrawCommand command);

        std::vector<DebugDrawCommand> commands_;
    };
} // namespace CoreEngine
