#pragma once
#include "core/ecs/node.h"
#include <string>
#include <utility>

namespace CoreEngine {
    class World {
    public:
        World() = default;

        ~World() = default;

        World(const World &) = delete;

        World &operator=(const World &) = delete;

        World(World &&) noexcept = default;

        World &operator=(World &&) noexcept = default;

        Node CreateNode(const std::string &name = "Node");

        void DestroyNode(Node node);

        [[nodiscard]] bool IsValid(Node node) const;

        template<typename... Components>
        [[nodiscard]] auto View();

        template<typename... Components>
        [[nodiscard]] auto View() const;

        [[nodiscard]] entt::registry &Registry();

        [[nodiscard]] const entt::registry &Registry() const;

        [[nodiscard]] std::size_t GetNodeCount() const;

        void Clear();

        template<typename T, typename... Args>
        T &Emplace(entt::entity e, Args &&... args);

        template<typename T>
        T &GetComponent(entt::entity e);

        template<typename T>
        const T &GetComponent(entt::entity e) const;

        template<typename T>
        T *TryGetComponent(entt::entity e);

        template<typename T>
        const T *TryGetComponent(entt::entity e) const;

        template<typename T>
        [[nodiscard]] bool HasComponent(entt::entity e) const;

        template<typename T>
        void RemoveComponent(entt::entity e);

    private:
        entt::registry registry_;
    };

    inline bool World::IsValid(Node node) const {
        return node.OwnerWorld() == this && registry_.valid(node.Handle());
    }

    template<typename... Components>
    auto World::View() {
        return registry_.view<Components...>();
    }

    template<typename... Components>
    auto World::View() const {
        return registry_.view<Components...>();
    }

    inline entt::registry &World::Registry() { return registry_; }

    inline const entt::registry &World::Registry() const { return registry_; }

    inline std::size_t World::GetNodeCount() const {
        return registry_.view<entt::entity>().size();
    }

    inline void World::Clear() { registry_.clear(); }

    template<typename T, typename... Args>
    T &World::Emplace(entt::entity e, Args &&... args) {
        return registry_.emplace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T>
    T &World::GetComponent(entt::entity e) {
        return registry_.get<T>(e);
    }

    template<typename T>
    const T &World::GetComponent(entt::entity e) const {
        return registry_.get<T>(e);
    }

    template<typename T>
    T *World::TryGetComponent(entt::entity e) {
        return registry_.try_get<T>(e);
    }

    template<typename T>
    const T *World::TryGetComponent(entt::entity e) const {
        return registry_.try_get<T>(e);
    }

    template<typename T>
    bool World::HasComponent(entt::entity e) const {
        return registry_.all_of<T>(e);
    }

    template<typename T>
    void World::RemoveComponent(entt::entity e) {
        registry_.remove<T>(e);
    }

    template<typename T, typename... Args>
    T &Node::AddComponent(Args &&... args) {
        return world_->Emplace<T>(handle_, std::forward<Args>(args)...);
    }

    template<typename T>
    T &Node::GetComponent() { return world_->GetComponent<T>(handle_); }

    template<typename T>
    const T &Node::GetComponent() const {
        return world_->GetComponent<T>(handle_);
    }

    template<typename T>
    T *Node::TryGetComponent() { return world_->TryGetComponent<T>(handle_); }

    template<typename T>
    const T *Node::TryGetComponent() const {
        return world_->TryGetComponent<T>(handle_);
    }

    template<typename T>
    bool Node::HasComponent() const { return world_->HasComponent<T>(handle_); }

    template<typename T>
    void Node::RemoveComponent() const { world_->RemoveComponent<T>(handle_); }
} // namespace CoreEngine
