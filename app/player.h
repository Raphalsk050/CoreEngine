#pragma once
#include "player_controller.h"
#include "player_pawn.h"
#include "third_person_camera_controller.h"
#include "core/network/player/network_player_system.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    struct EngineContext;
    struct FrameContext;
    class MultiplayerSystem;
    class RenderSystem;
    class World;
}

namespace Game {
    class Player {
    public:
        Player() = default;

        bool Initialize(const CoreEngine::EngineContext &context);

        void Update(const CoreEngine::FrameContext &frame);

        void Shutdown();

    private:
        [[nodiscard]] static CoreEngine::NetworkedPlayerMovementComponent MakeDefaultMovementComponent() noexcept;

        static void InitializeNetworkPlayerEntity(CoreEngine::NetworkPlayerEntityInitContext &context,
                                                  void *user_data);

        void ConfigureNetworkPlayerNode(CoreEngine::NetworkPlayerEntityInitContext &context);

        void AttachRemotePlayerModel(CoreEngine::World &world, CoreEngine::Node player_node);

        void CreatePawn(CoreEngine::World &world);

        void CreateCamera(CoreEngine::World &world);

        void LoadPlayerModel(const CoreEngine::EngineContext &context);

        void AttachController();

        CoreEngine::Node camera_node_;
        CoreEngine::Node player_renderer_node_;
        CoreEngine::Node player_model_root_;
        PlayerPawn player_pawn_;
        ThirdPersonCameraController third_person_camera_controller_;
        PlayerController player_controller_;
        CoreEngine::NetworkPlayerSystem *network_players_ = nullptr;
        CoreEngine::RenderSystem *render_system_ = nullptr;
        bool initialized_ = false;
    };
} // namespace Game
