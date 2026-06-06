#pragma once
#include <string>

namespace CoreEngine {
    struct TransformComponent;
    struct NameComponent;

    namespace Debug {
        void AppendDebugString(std::string &output, const TransformComponent &transform_component);

        void AppendDebugString(std::string &output, const NameComponent &name_component);

        // this is just to guarantee a good error log
        template<typename T>
        concept DebugStringWritable =
                requires(std::string &output, const T &value) { AppendDebugString(output, value); };

        template<DebugStringWritable T>
        [[nodiscard]] std::string ToString(const T &value) {
            std::string output;
            AppendDebugString(output, value);
            return output;
        }
    } // namespace Debug
} // namespace CoreEngine
