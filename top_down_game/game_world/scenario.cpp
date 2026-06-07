#include "scenario.h"

#include "core/ecs/components/directional_light_component.h"
#include "core/ecs/components/environment_light_component.h"
#include "core/ecs/world.h"
#include "core/render/render_system.h"

namespace CoreEngine {
    struct DirectionalLightComponent;
}

namespace TopDownGame {
    Scenario::Scenario(const CoreEngine::EngineContext &context) : context_(context) {
        InitializeScenario();
    }

    void Scenario::InitializeScenario() {
        auto floor_node = context_.world.CreateNode("Floor node");

        (void) context_.render_system.SetPrimitiveRenderer(
                floor_node, CoreEngine::PrimitiveRendererDesc::WithMaterial(
                                    CoreEngine::PrimitiveType::Cube,
                                    CoreEngine::Material::PbrStandard(
                                            CoreEngine::PbrStandardDesc::Linear(CoreEngine::Math::Vec4(0.7f)))));

        floor_node.SetScale(CoreEngine::Math::Vec3(5.0f, 0.02f, 5.0f));
        floor_node.SetPosition(CoreEngine::Math::Vec3(0.0f, -1.0f + 0.02f, 0.0f));

        InitializeSceneLights();
    }

    void Scenario::InitializeSceneLights() {
        auto sun = context_.world.CreateNode("Sun");

        sun.AddComponent<CoreEngine::DirectionalLightComponent>(CoreEngine::DirectionalLightComponent{
                .direction = CoreEngine::Math::Normalize(CoreEngine::Math::Vec3{-0.35f, -1.0f, 0.25f}),
                .illuminance_lux = 10000.0f,
                .color = {1.0f, 0.96f, 0.88f},
                .enabled = true,
        });

        sun.AddComponent<CoreEngine::EnvironmentLightComponent>(CoreEngine::EnvironmentLightComponent{
                .diffuse_irradiance = {0.55f, 0.62f, 0.75f},
                .intensity = 0.35f,
                .specular_radiance = {0.9f, 0.95f, 1.0f},
                .specular_intensity = 0.25f,
                .enabled = true,
        });
    }
} // namespace TopDownGame
