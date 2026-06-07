#include "scenario.h"

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
    Scenario::Scenario(const CoreEngine::EngineContext &context) : context_(context) {
        InitializeScenario();
    }

    void Scenario::InitializeScenario() {
        auto floor_node = context_.world.CreateNode("Floor node");

        // TODO(rafael): The engine now sucks because to creating a simple cube requires a bunch of shit and boilerplate
        // code. The API should be more ergonomic and user-friendly.
        // Mesh creation
        CoreEngine::MeshDesc mesh_desc = CoreEngine::Primitives::MeshFor(CoreEngine::PrimitiveType::Cube);
        CoreEngine::MeshHandle mesh = context_.render_system.CreateMesh(mesh_desc);

        // material creation
        CoreEngine::UnlitProps props{.color = CoreEngine::Math::Vec4(0.8f)};

        CoreEngine::Material material = CoreEngine::Material::Unlit(props);
        CoreEngine::MaterialHandle material_handle = material.Resolve(context_.render_system);

        floor_node.AddComponent<CoreEngine::MeshRendererComponent>(CoreEngine::MeshRendererComponent{
                .mesh = mesh,
                .material = material_handle,
                .visible = true,
                .cast_shadows = true,
                .topology = CoreEngine::PrimitiveTopology::TriangleList,
        });

        floor_node.SetScale(CoreEngine::Math::Vec3(5.0f, 0.02f, 5.0f));
        floor_node.SetPosition(CoreEngine::Math::Vec3(0.0f, -1.0f + 0.02f, 0.0f));
    }
} // namespace TopDownGame
