#include "core/debug/debug.h"
#include "core/ecs/components/name_component.h"
#include "core/ecs/components/transform_component.h"

namespace CoreEngine::Debug {
    void AppendDebugString(std::string &output, const TransformComponent &transform_component) {
        const Math::Vec3 &position = transform_component.Position();
        const Math::Quat &rotation = transform_component.Rotation();
        const Math::Vec3 &scale = transform_component.Scale();

        const std::string position_string = "Position: {" + std::to_string(position.x) + ", " +
                                            std::to_string(position.y) + ", " + std::to_string(position.z) + "} ";
        const std::string rotation_string = "Rotation: {" + std::to_string(rotation.x) + ", " +
                                            std::to_string(rotation.y) + ", " + std::to_string(rotation.z) + ", " +
                                            std::to_string(rotation.w) + "} ";

        const std::string scale_string = "Scale: {" + std::to_string(scale.x) + ", " + std::to_string(scale.y) + ", " +
                                         std::to_string(scale.z) + "} ";

        output = "\n" + position_string + "\n" + rotation_string + "\n" + scale_string;
    }

    void AppendDebugString(std::string &output, const NameComponent &name_component) { output = name_component.name; }
} // namespace CoreEngine::Debug
