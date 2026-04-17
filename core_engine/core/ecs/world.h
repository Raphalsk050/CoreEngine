#include "core/ecs/node.h"
#include <third_party/entt/entt.hpp>

namespace CoreEngine {
class World {
public:
  // --- Node Lifecycle ---
  Node CreateNode(const std::string &name = "Node");
  void DestroyNode(Node node);

  // --- Queries ---
  template <typename... Components> auto View();

  Node FindByName(const std::string &name);

  // --- Registry (advanced) ---
  entt::registry &Registry();

  // --- Stats ---
  std::size_t NodeCount() const;
  void Clear();
};
} // namespace CoreEngine
