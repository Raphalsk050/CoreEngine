#include <glm/glm.hpp>

namespace CoreEngine {
struct TransformComponent {
  double position[3] = {0.0, 0.0, 0.0};
  double rotation[4] = {0.0, 0.0, 0.0, 0.0};
  double scale[3] = {1.0, 1.0, 1.0};
};
} // namespace CoreEngine
