#include "player.h"

#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/world.h"
#include "core/render/material.h"
#include "core/render/primitives.h"
#include "core/render/render_system.h"

namespace TopDownGame {
    Player::Player(const CoreEngine::EngineContext &context) : Character(context) { Init(); }
    void Player::Init() {}
    void Player::Update(const CoreEngine::FrameContext &frame) {}
} // namespace TopDownGame
