#pragma once
#include <string>

namespace CoreEngine {
    struct NameComponent;
    struct TransformComponent;

    class Debug {
    public:
        template<typename T>
        [[nodiscard]] static std::string ToString(const T &value) {
            std::string output;
            AppendDebugString(output, value);
            return output;
        }

    private:
        static void AppendDebugString(std::string &output, const TransformComponent &transform_component);

        static void AppendDebugString(std::string &output, const NameComponent &name_component);
    };
} // CoreEngine
