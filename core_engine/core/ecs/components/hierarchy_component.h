#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace CoreEngine {
    struct HierarchyComponent {
        entt::entity parent = entt::null;
        entt::entity first_child = entt::null;
        entt::entity next_sibling = entt::null;
        entt::entity previous_sibling = entt::null;
        std::uint32_t child_count = 0;
    };
} // namespace CoreEngine
