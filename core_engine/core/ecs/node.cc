#include "core/ecs/node.h"

#include "components/hierarchy_component.h"
#include "components/name_component.h"
#include "components/transform_component.h"
#include "core/ecs/world.h"

namespace CoreEngine {
    namespace {
        struct TransformValues {
            Math::Vec3 position{0.f, 0.f, 0.f};
            Math::Quat rotation{1.f, 0.f, 0.f, 0.f};
            Math::Vec3 scale{1.f, 1.f, 1.f};
        };

        [[nodiscard]] bool IsValidEntity(const World *world, entt::entity entity) {
            return world != nullptr && entity != entt::null && world->Registry().valid(entity);
        }

        [[nodiscard]] HierarchyComponent &GetOrCreateHierarchy(World &world, entt::entity entity) {
            HierarchyComponent *hierarchy = world.TryGetComponent<HierarchyComponent>(entity);
            if (hierarchy != nullptr) {
                return *hierarchy;
            }

            return world.Emplace<HierarchyComponent>(entity);
        }

        [[nodiscard]] TransformValues DecomposeTransform(const Math::Mat4 &matrix) {
            TransformValues result;
            result.position = {matrix.data[12], matrix.data[13], matrix.data[14]};

            const Math::Vec3 basis_x{matrix.data[0], matrix.data[1], matrix.data[2]};
            const Math::Vec3 basis_y{matrix.data[4], matrix.data[5], matrix.data[6]};
            const Math::Vec3 basis_z{matrix.data[8], matrix.data[9], matrix.data[10]};

            result.scale = {
                Math::Length(basis_x),
                Math::Length(basis_y),
                Math::Length(basis_z),
            };

            Math::Mat4 rotation_matrix{1.f};
            if (result.scale.x > Math::Epsilon) {
                rotation_matrix.data[0] = matrix.data[0] / result.scale.x;
                rotation_matrix.data[1] = matrix.data[1] / result.scale.x;
                rotation_matrix.data[2] = matrix.data[2] / result.scale.x;
            }
            if (result.scale.y > Math::Epsilon) {
                rotation_matrix.data[4] = matrix.data[4] / result.scale.y;
                rotation_matrix.data[5] = matrix.data[5] / result.scale.y;
                rotation_matrix.data[6] = matrix.data[6] / result.scale.y;
            }
            if (result.scale.z > Math::Epsilon) {
                rotation_matrix.data[8] = matrix.data[8] / result.scale.z;
                rotation_matrix.data[9] = matrix.data[9] / result.scale.z;
                rotation_matrix.data[10] = matrix.data[10] / result.scale.z;
            }

            result.rotation = Math::Mat4ToQuat(rotation_matrix);
            return result;
        }

        void ApplyLocalTransform(Node node, const Math::Mat4 &local_matrix) {
            TransformValues transform = DecomposeTransform(local_matrix);
            TransformComponent &component = node.GetComponent<TransformComponent>();
            component.SetPosition(transform.position);
            component.SetRotation(transform.rotation);
            component.SetScale(transform.scale);
        }

        void DetachFromParent(Node node) {
            if (!node.IsValid()) {
                return;
            }

            HierarchyComponent &child_hierarchy = GetOrCreateHierarchy(*node.OwnerWorld(), node.Handle());
            if (child_hierarchy.parent == entt::null) {
                child_hierarchy.previous_sibling = entt::null;
                child_hierarchy.next_sibling = entt::null;
                return;
            }

            World *world = node.OwnerWorld();
            const entt::entity parent = child_hierarchy.parent;
            if (IsValidEntity(world, parent)) {
                HierarchyComponent &parent_hierarchy = GetOrCreateHierarchy(*world, parent);
                if (parent_hierarchy.first_child == node.Handle()) {
                    parent_hierarchy.first_child = child_hierarchy.next_sibling;
                }
                if (parent_hierarchy.child_count > 0u) {
                    --parent_hierarchy.child_count;
                }
            }

            if (IsValidEntity(world, child_hierarchy.previous_sibling)) {
                HierarchyComponent &previous = GetOrCreateHierarchy(*world, child_hierarchy.previous_sibling);
                previous.next_sibling = child_hierarchy.next_sibling;
            }

            if (IsValidEntity(world, child_hierarchy.next_sibling)) {
                HierarchyComponent &next = GetOrCreateHierarchy(*world, child_hierarchy.next_sibling);
                next.previous_sibling = child_hierarchy.previous_sibling;
            }

            child_hierarchy.parent = entt::null;
            child_hierarchy.previous_sibling = entt::null;
            child_hierarchy.next_sibling = entt::null;
        }

        void AttachToParent(Node child, Node parent) {
            HierarchyComponent &child_hierarchy = GetOrCreateHierarchy(*child.OwnerWorld(), child.Handle());
            HierarchyComponent &parent_hierarchy = GetOrCreateHierarchy(*parent.OwnerWorld(), parent.Handle());

            child_hierarchy.parent = parent.Handle();
            child_hierarchy.previous_sibling = entt::null;
            child_hierarchy.next_sibling = parent_hierarchy.first_child;

            if (IsValidEntity(child.OwnerWorld(), parent_hierarchy.first_child)) {
                HierarchyComponent &old_first_child = GetOrCreateHierarchy(*child.OwnerWorld(), parent_hierarchy.first_child);
                old_first_child.previous_sibling = child.Handle();
            }

            parent_hierarchy.first_child = child.Handle();
            ++parent_hierarchy.child_count;
        }
    } // namespace

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

    Node Node::GetParent() const {
        if (!IsValid()) {
            return {};
        }

        const HierarchyComponent *hierarchy = TryGetComponent<HierarchyComponent>();
        if (hierarchy == nullptr || hierarchy->parent == entt::null ||
            !world_->Registry().valid(hierarchy->parent)) {
            return {};
        }

        return Node{hierarchy->parent, world_};
    }

    Node Node::GetChild(std::size_t index) const {
        if (!IsValid()) {
            return {};
        }

        const HierarchyComponent *hierarchy = TryGetComponent<HierarchyComponent>();
        if (hierarchy == nullptr || index >= hierarchy->child_count) {
            return {};
        }

        entt::entity child = hierarchy->first_child;
        for (std::size_t current_index = 0; current_index < index; ++current_index) {
            if (!world_->Registry().valid(child)) {
                return {};
            }

            const HierarchyComponent *child_hierarchy = world_->TryGetComponent<HierarchyComponent>(child);
            if (child_hierarchy == nullptr) {
                return {};
            }

            child = child_hierarchy->next_sibling;
        }

        if (!world_->Registry().valid(child)) {
            return {};
        }

        return Node{child, world_};
    }

    std::uint32_t Node::GetChildCount() const {
        if (!IsValid()) {
            return 0;
        }

        const HierarchyComponent *hierarchy = TryGetComponent<HierarchyComponent>();
        return hierarchy == nullptr ? 0u : hierarchy->child_count;
    }

    bool Node::HasParent() const {
        return GetParent().IsValid();
    }

    bool Node::IsChildOf(Node parent) const {
        return parent.IsAncestorOf(*this);
    }

    bool Node::IsAncestorOf(Node child) const {
        if (!IsValid() || !child.IsValid() || world_ != child.world_) {
            return false;
        }

        constexpr std::uint32_t kMaxHierarchyDepth = 1024u;
        std::uint32_t depth = 0;
        Node current = child.GetParent();
        while (current.IsValid() && depth < kMaxHierarchyDepth) {
            if (current == *this) {
                return true;
            }

            current = current.GetParent();
            ++depth;
        }

        return false;
    }

    bool Node::SetParent(Node parent, bool keep_world_transform) {
        if (!IsValid()) {
            return false;
        }

        if (!parent.IsValid()) {
            ClearParent(keep_world_transform);
            return true;
        }

        if (world_ != parent.world_ || handle_ == parent.handle_ || IsAncestorOf(parent)) {
            return false;
        }

        const Math::Mat4 previous_world_matrix = keep_world_transform ? GetWorldMatrix() : Math::Identity();

        DetachFromParent(*this);
        AttachToParent(*this, parent);

        if (keep_world_transform) {
            const Math::Mat4 local_matrix = Math::Inverse(parent.GetWorldMatrix()) * previous_world_matrix;
            ApplyLocalTransform(*this, local_matrix);
        }

        return true;
    }

    void Node::ClearParent(bool keep_world_transform) {
        if (!IsValid()) {
            return;
        }

        const Math::Mat4 previous_world_matrix = keep_world_transform ? GetWorldMatrix() : Math::Identity();
        DetachFromParent(*this);

        if (keep_world_transform) {
            ApplyLocalTransform(*this, previous_world_matrix);
        }
    }

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

    Math::Mat4 Node::GetLocalMatrix() const {
        if (!IsValid()) {
            return Math::Identity();
        }

        return GetComponent<TransformComponent>().WorldMatrix();
    }

    Math::Mat4 Node::GetWorldMatrix() const {
        if (!IsValid()) {
            return Math::Identity();
        }

        constexpr std::uint32_t kMaxHierarchyDepth = 1024u;
        std::uint32_t depth = 0;
        Math::Mat4 world_matrix = GetLocalMatrix();

        const HierarchyComponent *hierarchy = TryGetComponent<HierarchyComponent>();
        entt::entity parent = hierarchy == nullptr ? entt::null : hierarchy->parent;
        while (parent != entt::null && world_->Registry().valid(parent) && depth < kMaxHierarchyDepth) {
            const TransformComponent *parent_transform = world_->TryGetComponent<TransformComponent>(parent);
            if (parent_transform != nullptr) {
                world_matrix = parent_transform->WorldMatrix() * world_matrix;
            }

            const HierarchyComponent *parent_hierarchy = world_->TryGetComponent<HierarchyComponent>(parent);
            parent = parent_hierarchy == nullptr ? entt::null : parent_hierarchy->parent;
            ++depth;
        }

        return world_matrix;
    }

    Math::Vec3 Node::GetWorldPosition() const {
        return DecomposeTransform(GetWorldMatrix()).position;
    }

    Math::Quat Node::GetWorldRotation() const {
        return DecomposeTransform(GetWorldMatrix()).rotation;
    }

    Math::Vec3 Node::GetWorldScale() const {
        return DecomposeTransform(GetWorldMatrix()).scale;
    }
} // namespace CoreEngine
