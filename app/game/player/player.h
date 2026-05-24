#pragma once

#include "game/camera/third_person_camera_controller.h"
#include "game/controller/player_controller.h"
#include "core/ecs/node.h"
#include "core/network/player/network_player_system.h"
#include "core/render/render_handle.h"
#include "game/player_weapon/player_weapon.h"

namespace CoreEngine {
    struct EngineContext;
    struct FixedFrameContext;
    struct FrameContext;
    class RenderSystem;
    class World;
}

namespace Game {
    /**
     * @brief Owns the local player entity, controller, camera, and network registration.
     *
     * Responsibility: assemble app-specific player presentation while keeping
     * movement and replicated ownership on the engine multiplayer path.
     */
    class Player final {
    public:
        Player() = default;

        [[nodiscard]] bool Initialize(const CoreEngine::EngineContext &context);

        void FixedUpdate(const CoreEngine::FixedFrameContext &frame);

        void Update(const CoreEngine::FrameContext &frame);

        void Shutdown();

        [[nodiscard]] CoreEngine::Node LocalNode() const noexcept {
            return player_node_;
        }

    private:
        [[nodiscard]] static CoreEngine::NetworkedPlayerMovementComponent MakeDefaultMovement() noexcept;

        static void InitializeNetworkPlayerEntity(CoreEngine::NetworkPlayerEntityInitContext &context,
                                                  void *user_data);

        void ConfigureNetworkPlayerEntity(CoreEngine::NetworkPlayerEntityInitContext &context);

        void CreateLocalPlayer(CoreEngine::World &world);

        void CreateCamera(CoreEngine::World &world);

        void LoadPlayerModel(const CoreEngine::EngineContext &context);

        void AttachPlayerModel(CoreEngine::World &world,
                               CoreEngine::Node parent,
                               const char *root_name,
                               CoreEngine::Node *out_root);

        CoreEngine::Node player_node_{};
        CoreEngine::Node player_renderer_node_{};
        CoreEngine::Node player_model_root_{};
        CoreEngine::Node camera_node_{};
        ThirdPersonCameraController camera_controller_{};
        PlayerController controller_{};
        CoreEngine::NetworkPlayerSystem *network_players_ = nullptr;
        CoreEngine::RenderSystem *render_system_ = nullptr;
        CoreEngine::ModelHandle player_model_{};
        PlayerWeapon player_weapon_{};
        bool initialized_ = false;
    };
} // Game
