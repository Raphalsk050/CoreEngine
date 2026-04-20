#pragma once
#include <cstdint>
#include <entt/entt.hpp>

#include "glm/fwd.hpp"
#include "glm/vec3.hpp"

namespace CoreEngine {
    class World;

    class Node {
    public:
        Node() = default;

        Node(entt::entity handle, World *world);

        template<typename T, typename... Args>
        T &AddComponent(Args &&... args);

        template<typename T>
        T &GetComponent();

        template<typename T>
        const T &GetComponent() const;

        template<typename T>
        T *TryGetComponent();

        template<typename T>
        const T *TryGetComponent() const;

        template<typename T>
        bool HasComponent() const;

        template<typename T>
        void RemoveComponent() const;

        [[nodiscard]] std::string GetName() const;

        void Destroy();

        [[nodiscard]] bool IsValid() const;

        [[nodiscard]] uint32_t Id() const;

        [[nodiscard]] entt::entity Handle() const { return handle_; }

        explicit operator bool() const;

        bool operator==(const Node &other) const;

        bool operator!=(const Node &other) const;

        void SetPosition(const glm::vec3 &position);

        void SetRotation(const glm::quat &rotation);

        void SetScale(const glm::vec3 &scale);

    private:
        entt::entity handle_{entt::null};
        World *world_{nullptr};
    };
} // namespace CoreEngine
