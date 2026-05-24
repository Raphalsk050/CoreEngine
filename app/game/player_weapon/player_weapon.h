#pragma once
#include <array>
#include <cstdint>

#include "core/application/fixed_frame_context.h"
#include "core/application/frame_context.h"
#include "core/debug/debug_draw.h"
#include "core/ecs/node.h"
#include "core/math/math.h"
#include "core/network/network_input_command_queue.h"
#include "core/network/replication/network_identity_component.h"
#include "game/camera/third_person_camera_controller.h"

namespace Game {
    class ThirdPersonCameraController;

    /**
     * @brief Owns app-specific weapon firing for the local player presentation.
     *
     * Responsibility: consume engine player commands and render fire traces while
     * keeping weapon gameplay out of the engine core networking layer.
     */
    class PlayerWeapon final {
    public:
        void AttachCameraController(ThirdPersonCameraController &camera_controller) noexcept;

        void DetachCameraController() noexcept;

        void FixedUpdate(const CoreEngine::FixedFrameContext &frame) noexcept;

        void Update(const CoreEngine::FrameContext &frame) noexcept;

    private:
        struct FireCursor {
            CoreEngine::NetworkEntityId network_id = 0;
            std::uint32_t last_sequence = 0;
            double next_fire_time = 0.0;
        };

        [[nodiscard]] bool TryConsumeFire(const CoreEngine::QueuedPlayerInputCommand &queued,
                                          double simulation_time) noexcept;

        [[nodiscard]] CoreEngine::Math::Vec3 ResolveFireOrigin(
            CoreEngine::Node shooter,
            bool local_player) const noexcept;

        [[nodiscard]] CoreEngine::Math::Vec3 ResolveFireDirection(
            const CoreEngine::QueuedPlayerInputCommand &queued,
            bool local_player) const noexcept;

        void DrawFireTrace(CoreEngine::DebugDrawSystem &debug_draw,
                           const CoreEngine::Math::Vec3 &origin,
                           const CoreEngine::Math::Vec3 &direction,
                           bool predicted) const;

        void ProcessReplicatedFireTraces(const CoreEngine::FixedFrameContext &frame) const noexcept;

        void BroadcastFireTrace(const CoreEngine::FixedFrameContext &frame,
                                const CoreEngine::QueuedPlayerInputCommand &queued,
                                const CoreEngine::Math::Vec3 &origin,
                                const CoreEngine::Math::Vec3 &direction) const noexcept;

        static constexpr std::size_t kMaxFireCursors = 16;
        std::array<FireCursor, kMaxFireCursors> fire_cursors_{};
        ThirdPersonCameraController *camera_controller_ = nullptr;
    };
} // Game

