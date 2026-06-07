#include "game_app.h"
#include "controller/controller.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/world.h"
#include "core/render/material.h"
#include "core/render/mesh_desc.h"
#include "core/render/primitive_topology.h"
#include "core/render/primitives.h"
#include "core/render/render_handle.h"
#include "core/render/render_system.h"

namespace CoreEngine {
    struct MeshRendererComponent;
}

namespace TopDownGame {
    void GameApp::Init(const CoreEngine::EngineContext &context) {
        controller_ = std::make_unique<Controller>(context);
        player_ = std::make_unique<Player>(context);
        scenario_ = std::make_unique<Scenario>(context);

        controller_->Possess(*player_);
    }

    void GameApp::Update(const CoreEngine::FrameContext &frame) {
        if (controller_ != nullptr) {
            controller_->Update(frame);
        }
    }

    void GameApp::Shutdown(const CoreEngine::EngineContext &context) {
    }
} // namespace TopDownGame
