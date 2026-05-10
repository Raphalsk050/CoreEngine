#pragma once

#include "core/math/math.h"

namespace CoreEngine {
    struct TransformComponent {
        Math::Vec3 position = Math::Vec3(0.0, 0.0, 0.0);
        Math::Quat rotation = Math::Quat(1.0, 0.0, 0.0, 0.0);
        Math::Vec3 scale    = Math::Vec3(1.0, 1.0, 1.0);

        explicit TransformComponent(Math::Vec3 position = Math::Vec3(0.0, 0.0, 0.0),
                                    Math::Quat rotation = Math::Quat(1.0, 0.0, 0.0, 0.0),
                                    Math::Vec3 scale    = Math::Vec3(1.0, 1.0, 1.0))
            : position(position), rotation(rotation), scale(scale) {}

        void SetPosition(const Math::Vec3 &value) noexcept {
            position = value;
            dirty_ = true;
        }

        void SetRotation(const Math::Quat &value) noexcept {
            rotation = value;
            dirty_ = true;
        }

        void SetScale(const Math::Vec3 &value) noexcept {
            scale = value;
            dirty_ = true;
        }

        [[nodiscard]] const Math::Vec3 &Position() const noexcept {
            return position;
        }

        [[nodiscard]] const Math::Quat &Rotation() const noexcept {
            return rotation;
        }

        [[nodiscard]] const Math::Vec3 &Scale() const noexcept {
            return scale;
        }

        void MarkDirty() noexcept {
            dirty_ = true;
        }

        [[nodiscard]] const Math::Mat4 &WorldMatrix() const {
            if (dirty_ || !Equals(cached_position_, position) || !Equals(cached_rotation_, rotation) ||
                !Equals(cached_scale_, scale)) {
                cached_world_matrix_ = Math::ComposeTransform(position, rotation, scale);
                cached_position_ = position;
                cached_rotation_ = rotation;
                cached_scale_ = scale;
                dirty_ = false;
            }

            return cached_world_matrix_;
        }

    private:
        [[nodiscard]] static bool Equals(const Math::Vec3 &lhs, const Math::Vec3 &rhs) noexcept {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
        }

        [[nodiscard]] static bool Equals(const Math::Quat &lhs, const Math::Quat &rhs) noexcept {
            return lhs.w == rhs.w && lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
        }

        mutable Math::Mat4 cached_world_matrix_{1.f};
        mutable Math::Vec3 cached_position_{0.f, 0.f, 0.f};
        mutable Math::Quat cached_rotation_{1.f, 0.f, 0.f, 0.f};
        mutable Math::Vec3 cached_scale_{1.f, 1.f, 1.f};
        mutable bool dirty_ = true;
    };
} // namespace CoreEngine
