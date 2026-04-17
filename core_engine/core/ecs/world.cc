#include "core/ecs/world.h"

namespace CoreEngine {
Node World::CreateNode(const std::string &name) {
  entt::entity handle = registry_.create();
  return Node{handle, this};
}

void World::DestroyNode(Node node) {
  if (IsValid(node)) {
    registry_.destroy(node.Handle());
  }
}
} // namespace CoreEngine
