#pragma once
#include "core/ecs/node.h"
#include <string>
#include <third_party/entt/entt.hpp>
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

  template <typename... Components> [[nodiscard]] auto View();
  template <typename... Components> [[nodiscard]] auto View() const;

  [[nodiscard]] entt::registry &Registry();
  [[nodiscard]] const entt::registry &Registry() const;

  [[nodiscard]] std::size_t NodeCount() const;
  void Clear();

  template <typename T, typename... Args>
  T &Emplace(entt::entity e, Args &&...args);
  template <typename T> T &Get(entt::entity e);
  template <typename T> const T &Get(entt::entity e) const;
  template <typename T> T *TryGet(entt::entity e);
  template <typename T> const T *TryGet(entt::entity e) const;
  template <typename T> bool Has(entt::entity e) const;
  template <typename T> void Remove(entt::entity e);

private:
  entt::registry registry_;
};

inline bool World::IsValid(Node node) const {
  return registry_.valid(node.Handle());
}

template <typename... Components> auto World::View() {
  return registry_.view<Components...>();
}

template <typename... Components> auto World::View() const {
  return registry_.view<Components...>();
}

inline entt::registry &World::Registry() { return registry_; }
inline const entt::registry &World::Registry() const { return registry_; }

inline std::size_t World::NodeCount() const {
  return registry_.storage<entt::entity>()->free_list();
}

inline void World::Clear() { registry_.clear(); }

template <typename T, typename... Args>
T &World::Emplace(entt::entity e, Args &&...args) {
  return registry_.emplace<T>(e, std::forward<Args>(args)...);
}

template <typename T> T &World::Get(entt::entity e) {
  return registry_.get<T>(e);
}

template <typename T> const T &World::Get(entt::entity e) const {
  return registry_.get<T>(e);
}

template <typename T> T *World::TryGet(entt::entity e) {
  return registry_.try_get<T>(e);
}

template <typename T> const T *World::TryGet(entt::entity e) const {
  return registry_.try_get<T>(e);
}

template <typename T> bool World::Has(entt::entity e) const {
  return registry_.all_of<T>(e);
}

template <typename T> void World::Remove(entt::entity e) {
  registry_.remove<T>(e);
}

template <typename T, typename... Args> T &Node::Add(Args &&...args) {
  return world_->Emplace<T>(handle_, std::forward<Args>(args)...);
}

template <typename T> T &Node::Get() { return world_->Get<T>(handle_); }

template <typename T> const T &Node::Get() const {
  return world_->Get<T>(handle_);
}

template <typename T> T *Node::TryGet() { return world_->TryGet<T>(handle_); }

template <typename T> const T *Node::TryGet() const {
  return world_->TryGet<T>(handle_);
}

template <typename T> bool Node::Has() const { return world_->Has<T>(handle_); }

template <typename T> void Node::Remove() { world_->Remove<T>(handle_); }
} // namespace CoreEngine
