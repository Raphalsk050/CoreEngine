#include "core/debug/debug_draw.h"

#include <algorithm>

namespace CoreEngine {
    void DebugDrawSystem::BeginFrame(float delta_seconds) {
        const float safe_delta = std::max(delta_seconds, 0.0f);
        for (DebugDrawCommand &command: commands_) {
            command.remaining_seconds -= safe_delta;
        }

        commands_.erase(
            std::remove_if(commands_.begin(),
                           commands_.end(),
                           [](const DebugDrawCommand &command) {
                               return command.remaining_seconds <= 0.0f;
                           }),
            commands_.end());
    }

    void DebugDrawSystem::Clear() noexcept {
        commands_.clear();
    }

    void DebugDrawSystem::DrawLine(const Math::Vec3 &start,
                                   const Math::Vec3 &end,
                                   const DebugDrawStyle &style) {
        Push(DebugDrawCommand{
            .shape = DebugDrawShapeType::Line,
            .start = start,
            .end = end,
            .style = style,
        });
    }

    void DebugDrawSystem::DrawBox(const Math::Vec3 &center,
                                  const Math::Vec3 &half_extents,
                                  const Math::Quat &rotation,
                                  const DebugDrawStyle &style) {
        Push(DebugDrawCommand{
            .shape = DebugDrawShapeType::Box,
            .center = center,
            .half_extents = half_extents,
            .rotation = rotation,
            .style = style,
        });
    }

    void DebugDrawSystem::DrawSphere(const Math::Vec3 &center,
                                     float radius,
                                     const DebugDrawStyle &style) {
        if (radius <= 0.0f) {
            return;
        }

        Push(DebugDrawCommand{
            .shape = DebugDrawShapeType::Sphere,
            .center = center,
            .radius = radius,
            .style = style,
        });
    }

    void DebugDrawSystem::DrawLineTrace(const Math::Vec3 &start,
                                        const Math::Vec3 &end,
                                        const DebugTraceResult &result,
                                        const DebugDrawStyle &style) {
        DrawLine(start, result.hit ? result.point : end, style);
        if (result.hit) {
            DrawSphere(result.point, 0.08f, DebugDrawStyle{
                .color = {1.0f, 0.1f, 0.1f, 1.0f},
                .duration_seconds = style.duration_seconds,
                .depth_test = style.depth_test,
            });
            DrawLine(result.point, result.point + result.normal * 0.35f, DebugDrawStyle{
                .color = {1.0f, 1.0f, 0.0f, 1.0f},
                .duration_seconds = style.duration_seconds,
                .depth_test = style.depth_test,
            });
        }
    }

    void DebugDrawSystem::DrawBoxTrace(const Math::Vec3 &start,
                                       const Math::Vec3 &end,
                                       const Math::Vec3 &half_extents,
                                       const Math::Quat &rotation,
                                       const DebugTraceResult &result,
                                       const DebugDrawStyle &style) {
        DrawLineTrace(start, end, result, style);
        DrawBox(start, half_extents, rotation, style);
        DrawBox(result.hit ? result.point : end, half_extents, rotation, style);
    }

    void DebugDrawSystem::DrawSphereTrace(const Math::Vec3 &start,
                                          const Math::Vec3 &end,
                                          float radius,
                                          const DebugTraceResult &result,
                                          const DebugDrawStyle &style) {
        DrawLineTrace(start, end, result, style);
        DrawSphere(start, radius, style);
        DrawSphere(result.hit ? result.point : end, radius, style);
    }

    void DebugDrawSystem::Push(DebugDrawCommand command) {
        command.remaining_seconds = command.style.duration_seconds > 0.0f
                                        ? command.style.duration_seconds
                                        : 0.0001f;
        commands_.push_back(command);
    }
} // namespace CoreEngine
