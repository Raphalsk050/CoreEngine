#include "core/ecs/world.h"
#include <third_party/entt/entt.hpp>

namespace CoreEngine {

class IWorldService {
public:
  virtual ~IWorldService() = default;
  virtual World &GetWorld() = 0;
  virtual const World &GetWorld() const = 0;
};

class WorldAccess {
public:
  static void Bind(IWorldService &service);
  static void Unbind();

  static World &Get();
};
} // namespace CoreEngine
