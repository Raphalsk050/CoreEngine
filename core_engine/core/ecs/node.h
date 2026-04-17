#pragma once
#include <cstdint>
#include <third_party/entt/entt.hpp>

namespace CoreEngine {
class World;

class Node {
public:
  Node() = default;
  Node(entt::entity handle, World *world);

  template <typename T, typename... Args> T &Add(Args &&...args);
  template <typename T> T &Get();
  template <typename T> const T &Get() const;
  template <typename T> T *TryGet();
  template <typename T> const T *TryGet() const;
  template <typename T> bool Has() const;
  template <typename T> void Remove();

  void Destroy();
  [[nodiscard]] bool IsValid() const;
  [[nodiscard]] uint32_t Id() const;
  [[nodiscard]] entt::entity Handle() const { return handle_; }

  explicit operator bool() const;
  bool operator==(const Node &other) const;
  bool operator!=(const Node &other) const;

private:
  entt::entity handle_{entt::null};
  World *world_{nullptr};
};
} // namespace CoreEngine