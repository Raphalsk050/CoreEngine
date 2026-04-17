#include "core/ecs/node.h"
#include "core/ecs/world.h"

namespace CoreEngine {
Node::Node(entt::entity handle, World *world) : handle_(handle), world_(world) {}

void Node::Destroy() {
  if (IsValid()) {
    world_->DestroyNode(*this);
    handle_ = entt::null;
    world_ = nullptr;
  }
}

bool Node::IsValid() const {
  return world_ != nullptr && world_->IsValid(*this);
}

uint32_t Node::Id() const { return static_cast<uint32_t>(handle_); }

Node::operator bool() const { return IsValid(); }

bool Node::operator==(const Node &other) const {
  return handle_ == other.handle_ && world_ == other.world_;
}

bool Node::operator!=(const Node &other) const { return !(*this == other); }
} // namespace CoreEngine