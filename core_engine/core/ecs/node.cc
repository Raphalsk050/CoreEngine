#include "core/ecs/node.h"

#include "components/name_component.h"
#include "components/transform_component.h"
#include "core/ecs/world.h"

namespace CoreEngine {
    Node::Node(entt::entity handle, World *world) : handle_(handle), world_(world) {
    }

    std::string Node::GetName() const {
        return GetComponent<NameComponent>().name;
    }

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

    void Node::SetPosition(const Math::Vec3 &position) {
        GetComponent<TransformComponent>().SetPosition(position);
    }

    void Node::SetRotation(const Math::Quat &rotation) {
        GetComponent<TransformComponent>().SetRotation(rotation);
    }

    void Node::SetScale(const Math::Vec3 &scale) {
        GetComponent<TransformComponent>().SetScale(scale);
    }

    Math::Vec3 Node::GetPosition() const {
        return GetComponent<TransformComponent>().Position();
    }

    Math::Quat Node::GetRotation() const {
        return GetComponent<TransformComponent>().Rotation();
    }

    Math::Vec3 Node::GetScale() const {
        return GetComponent<TransformComponent>().Scale();
    }
} // namespace CoreEngine
