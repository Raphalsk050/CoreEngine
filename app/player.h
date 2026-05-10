#pragma once
#include "player_controller.h"
#include "player_pawn.h"
#include "third_person_camera_controller.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    struct EngineContext;
    struct FrameContext;
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
        void CreatePawn(CoreEngine::World &world);

        void CreateCamera(CoreEngine::World &world);

        void LoadPlayerTexture(CoreEngine::RenderSystem &render_system);

        [[nodiscard]] CoreEngine::MaterialHandle LoadPlayerMaterial(CoreEngine::RenderSystem &render_system) const;

        void LoadPlayerModel(const CoreEngine::EngineContext &context,
                             CoreEngine::MaterialHandle player_material);

        void AddPlayerComponents(CoreEngine::MeshHandle mesh,
                                 CoreEngine::MaterialHandle material);

        void AttachController();

        CoreEngine::TextureHandle player_texture_;
        CoreEngine::Node camera_node_;
        CoreEngine::Node player_renderer_node_;
        PlayerPawn player_pawn_;
        ThirdPersonCameraController third_person_camera_controller_;
        PlayerController player_controller_;
        bool initialized_ = false;
    };
} // namespace Game
