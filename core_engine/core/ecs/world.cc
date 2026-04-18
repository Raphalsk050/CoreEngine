#include "core/ecs/world.h"
#include "core/ecs/components/name_component.h"
#include "core/ecs/components/transform_component.h"

namespace CoreEngine {
Node World::CreateNode(const std::string &name) {
  entt::entity handle = registry_.create();
  Node node = Node{handle, this};
  node.AddComponent<TransformComponent>(TransformComponent{});
  node.AddComponent<NameComponent>(NameComponent{.name = name});
  return node;
}

void World::DestroyNode(Node node) {
  if (IsValid(node)) {
    registry_.destroy(node.Handle());
  }
}
} // namespace CoreEngine
