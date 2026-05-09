#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <entt/entt.hpp>

#include "core/math/math.h"

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

        [[nodiscard]] World *OwnerWorld() const { return world_; }

        explicit operator bool() const;

        bool operator==(const Node &other) const;

        bool operator!=(const Node &other) const;

        [[nodiscard]] Node GetParent() const;

        [[nodiscard]] Node GetChild(std::size_t index) const;

        [[nodiscard]] std::uint32_t GetChildCount() const;

        [[nodiscard]] bool HasParent() const;

        [[nodiscard]] bool IsChildOf(Node parent) const;

        [[nodiscard]] bool IsAncestorOf(Node child) const;

        bool SetParent(Node parent, bool keep_world_transform = false);

        void ClearParent(bool keep_world_transform = false);

        void SetPosition(const Math::Vec3 &position);

        void SetRotation(const Math::Quat &rotation);

        void SetScale(const Math::Vec3 &scale);

        Math::Vec3 GetPosition() const;

        Math::Quat GetRotation() const;

        Math::Vec3 GetScale() const;

        [[nodiscard]] Math::Mat4 GetLocalMatrix() const;

        [[nodiscard]] Math::Mat4 GetWorldMatrix() const;

        [[nodiscard]] Math::Vec3 GetWorldPosition() const;

        [[nodiscard]] Math::Quat GetWorldRotation() const;

        [[nodiscard]] Math::Vec3 GetWorldScale() const;

    private:
        entt::entity handle_{entt::null};
        World *world_{nullptr};
    };
} // namespace CoreEngine
