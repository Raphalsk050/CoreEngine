#include "core/ecs/world.h"
#include "world.h"

namespace CoreEngine {
Node World::CreateNode(const std::string &name) { return Node(); }

void World::DestroyNode(Node node) {}

Node World::FindByName(const std::string &name) { return Node(); }

} // namespace CoreEngine
