#include "character.h"

#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/world.h"
#include "core/render/material.h"
#include "core/render/mesh_desc.h"
#include "core/render/primitives.h"
#include "core/render/render_system.h"

namespace TopDownGame {

    Character::Character(const CoreEngine::EngineContext &context) : context_(context) {

        character_node_ = context_.world.CreateNode("Character");
        InitializeCharacterRenderer();
    }

    void Character::OnPossessed() {
        is_possessed_ = true;
    }

    void Character::OnUnpossessed() {
        is_possessed_ = false;
    }

    void Character::AddMovementInput(CoreEngine::InputVector2 input) {
        last_input_ = current_input_;
        current_input_ = input;

        // TODO(rafael): In the future, this direct position-setting approach should probably be handled by the
        // physics system. Something like AddVelocity() or a similar method would be preferable.
        // This would be better than the current approach. This would be better than this shitty way.
        if (character_node_.IsValid()) {
            character_node_.SetPosition(character_node_.GetPosition() +
                                        CoreEngine::Math::Vec3(current_input_.x, 0.0f, current_input_.y));
        }
    }

    void Character::InitializeCharacterRenderer() {
        // TODO(rafael): The engine now sucks because to creating a simple cube requires a bunch of shit and boilerplate
        // code. The API should be more ergonomic and user-friendly.
        // Mesh creation
        CoreEngine::MeshDesc mesh_desc = CoreEngine::Primitives::MeshFor(CoreEngine::PrimitiveType::Cube);
        CoreEngine::MeshHandle mesh = context_.render_system.CreateMesh(mesh_desc);

        // material creation
        CoreEngine::UnlitProps props{.color = CoreEngine::Math::Vec4(1.0f)};

        CoreEngine::Material material = CoreEngine::Material::Unlit(props);
        CoreEngine::MaterialHandle material_handle = material.Resolve(context_.render_system);

        character_node_.AddComponent<CoreEngine::MeshRendererComponent>(CoreEngine::MeshRendererComponent{
                .mesh = mesh,
                .material = material_handle,
                .visible = true,
                .cast_shadows = true,
                .topology = CoreEngine::PrimitiveTopology::TriangleList,
        });
    }
} // namespace TopDownGame
