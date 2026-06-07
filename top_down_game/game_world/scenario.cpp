#include "scenario.h"

#include "core/ecs/world.h"
#include "core/render/render_system.h"

namespace TopDownGame {
    Scenario::Scenario(const CoreEngine::EngineContext &context) : context_(context) {
        InitializeScenario();
    }

    void Scenario::InitializeScenario() {
        auto floor_node = context_.world.CreateNode("Floor node");

        (void) context_.render_system.SetPrimitiveRenderer(
                floor_node,
                CoreEngine::PrimitiveRendererDesc::Unlit(CoreEngine::PrimitiveType::Cube, CoreEngine::Math::Vec4(0.8f)));

        floor_node.SetScale(CoreEngine::Math::Vec3(5.0f, 0.02f, 5.0f));
        floor_node.SetPosition(CoreEngine::Math::Vec3(0.0f, -1.0f + 0.02f, 0.0f));
    }
} // namespace TopDownGame
