#pragma once
#include "player_controller.h"
#include "player_pawn.h"
#include "third_person_camera_controller.h"
#include "core/network/replication/network_identity_component.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    struct EngineContext;
    struct FrameContext;
    struct SimulationFrame;
    class NetworkPredictionSystem;
    class NetworkSystem;
    class RenderSystem;
    class World;
}

namespace Game {
    class Player {
    public:
        Player() = default;

        bool Initialize(const CoreEngine::EngineContext &context);

        void Update(const CoreEngine::FrameContext &frame);

        void FixedUpdate(const CoreEngine::SimulationFrame &frame);

        void Shutdown();

    private:
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
        CoreEngine::NetworkPredictionSystem *prediction_system_ = nullptr;
        CoreEngine::NetworkSystem *network_system_ = nullptr;
        CoreEngine::NetworkEntityId network_entity_id_ = 0;
        std::uint32_t last_reconciled_input_sequence_ = 0;
        bool initialized_ = false;
    };
} // namespace Game
