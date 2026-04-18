#include "core/debug/debug.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/name_component.h"

namespace CoreEngine::Debug {
    void AppendDebugString(std::string &output, const TransformComponent &transform_component) {
        const std::string position_string = "Position: {" + std::to_string(transform_component.position.x) +
                                            ", " + std::to_string(transform_component.position.y) + ", " +
                                            std::to_string(transform_component.position.z) + "} ";
        const std::string rotation_string = "Rotation: {" + std::to_string(transform_component.rotation.x) +
                                            ", " + std::to_string(transform_component.rotation.y) + ", " +
                                            std::to_string(transform_component.rotation.z) + ", " +
                                            std::to_string(transform_component.rotation.w) + "} ";

        const std::string scale_string = "Scale: {" + std::to_string(transform_component.scale.x) + ", " +
                                         std::to_string(transform_component.scale.y) + ", " +
                                         std::to_string(transform_component.scale.z) + "} ";

        output =
                "\n" + position_string + "\n" + rotation_string + "\n" + scale_string;
    }

    void AppendDebugString(std::string &output, const NameComponent &name_component) {
        output = name_component.name;
    }
} // CoreEngine
