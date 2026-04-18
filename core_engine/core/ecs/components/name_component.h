#pragma once
#include <string>

namespace CoreEngine {
    struct NameComponent {
        std::string name{"Node"};

        explicit NameComponent(std::string_view value = "Node") : name(value) {
        }
    };
} // namespace CoreEngine
