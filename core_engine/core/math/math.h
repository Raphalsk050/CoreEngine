#pragma once

// =============================================================================
// CoreEngine::Math — Thin, zero-cost abstraction over GLM.
//
// Goals:
//   1. Isolate GLM headers from domain/application code.
//   2. Provide engine-standard naming (PascalCase) for types and functions.
//   3. All wrappers are inline/constexpr — zero runtime overhead.
//   4. Concepts constrain templates for clear compiler errors.
//
// Usage:
//   #include "core/math/math.h"
//   auto v = CoreEngine::Math::Normalize(CoreEngine::Math::Vec3{1,2,3});
//
// WARNING: With GLM_FORCE_DEFAULT_ALIGNED_GENTYPES, sizeof(Vec3) == 16 (not 12).
//          Be mindful of this when defining GPU-facing structs.
// =============================================================================

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>

#include <concepts>
#include <type_traits>

namespace CoreEngine::Math {
    using Vec2 = glm::vec2;
    using Vec3 = glm::vec3;
    using Vec4 = glm::vec4;
    using Mat2 = glm::mat2;
    using Mat3 = glm::mat3;
    using Mat4 = glm::mat4;
    using Quat = glm::quat;

    using IVec2 = glm::ivec2;
    using IVec3 = glm::ivec3;
    using IVec4 = glm::ivec4;
    using UVec2 = glm::uvec2;
    using UVec3 = glm::uvec3;
    using UVec4 = glm::uvec4;
    using DVec2 = glm::dvec2;
    using DVec3 = glm::dvec3;
    using DVec4 = glm::dvec4;

    /// Scalar types accepted by GLM math functions.
    template<typename T>
    concept Scalar = std::same_as<T, float>
                     || std::same_as<T, double>
                     || std::same_as<T, int>;

    /// Any GLM vector type (has value_type and .x member).
    template<typename T>
    concept Vector = requires(T v)
    {
        typename T::value_type;
        requires Scalar<typename T::value_type>;
        { v.x } -> std::convertible_to<float>;
    };

    /// Types that support glm::dot (vectors).
    template<typename T>
    concept DotCompatible = requires(T const &a, T const &b)
    {
        typename T::value_type;
        requires Scalar<typename T::value_type>;
        { glm::dot(a, b) } -> std::convertible_to<typename T::value_type>;
    };

    /// Types that support glm::cross (vec3 only).
    template<typename T>
    concept CrossCompatible = requires(T const &a, T const &b)
    {
        typename T::value_type;
        requires Scalar<typename T::value_type>;
        { glm::cross(a, b) } -> std::same_as<T>;
    };

    /// Types valid for angle conversion (scalars and vectors).
    template<typename T>
    concept AngleConvertible = Scalar<T> || Vector<T>;

    inline constexpr float Pi      = 3.14159265358979323846f;
    inline constexpr float TwoPi   = 6.28318530717958647692f;
    inline constexpr float HalfPi  = 1.57079632679489661923f;
    inline constexpr float Epsilon = 1.192092896e-07f;

    /// Converts degrees to radians.
    template<AngleConvertible T>
    [[nodiscard]] constexpr T Deg2Rad(T degrees) noexcept { return glm::radians(degrees); }

    /// Converts radians to degrees.
    template<AngleConvertible T>
    [[nodiscard]] constexpr T Rad2Deg(T radians) noexcept { return glm::degrees(radians); }

    /// Dot product of two vectors.
    template<DotCompatible T>
    [[nodiscard]] inline auto Dot(T const &a, T const &b) noexcept -> typename T::value_type {
        return glm::dot(a, b);
    }

    /// Cross product (vec3 only).
    template<CrossCompatible T>
    [[nodiscard]] inline T Cross(T const &a, T const &b) noexcept {
        return glm::cross(a, b);
    }

    /// Returns a normalized copy of the vector.
    template<DotCompatible T>
    [[nodiscard]] inline T Normalize(T const &v) noexcept {
        return glm::normalize(v);
    }

    /// Euclidean length of a vector.
    template<DotCompatible T>
    [[nodiscard]] inline auto Length(T const &v) noexcept -> typename T::value_type {
        return glm::length(v);
    }

    /// Squared length of a vector (avoids sqrt — prefer for comparisons).
    template<DotCompatible T>
    [[nodiscard]] inline auto LengthSquared(T const &v) noexcept -> typename T::value_type {
        return glm::dot(v, v);
    }

    /// Euclidean distance between two points.
    template<DotCompatible T>
    [[nodiscard]] inline auto Distance(T const &a, T const &b) noexcept -> typename T::value_type {
        return glm::distance(a, b);
    }

    /// Reflects vector I around normal N.
    template<DotCompatible T>
    [[nodiscard]] inline T Reflect(T const &incident, T const &normal) noexcept {
        return glm::reflect(incident, normal);
    }

    /// Linear interpolation between a and b by factor t.
    template<typename T>
    [[nodiscard]] inline T Lerp(T const &a, T const &b, float t) noexcept {
        return glm::mix(a, b, t);
    }

    /// Spherical linear interpolation for quaternions.
    [[nodiscard]] inline Quat Slerp(Quat const &a, Quat const &b, float t) noexcept {
        return glm::slerp(a, b, t);
    }

    /// Hermite smoothstep interpolation.
    [[nodiscard]] inline float SmoothStep(float edge0, float edge1, float x) noexcept {
        return glm::smoothstep(edge0, edge1, x);
    }

    /// Clamps value between min and max.
    template<typename T>
    [[nodiscard]] inline T Clamp(T const &value, T const &min_val, T const &max_val) noexcept {
        return glm::clamp(value, min_val, max_val);
    }

    /// Component-wise minimum.
    template<typename T>
    [[nodiscard]] inline T Min(T const &a, T const &b) noexcept {
        return glm::min(a, b);
    }

    /// Component-wise maximum.
    template<typename T>
    [[nodiscard]] inline T Max(T const &a, T const &b) noexcept {
        return glm::max(a, b);
    }

    /// Absolute value (scalar or component-wise).
    template<typename T>
    [[nodiscard]] inline T Abs(T const &v) noexcept {
        return glm::abs(v);
    }

    /// Returns a 4x4 identity matrix.
    [[nodiscard]] inline Mat4 Identity() noexcept {
        return Mat4(1.f);
    }

    /// Applies a translation to matrix m.
    [[nodiscard]] inline Mat4 Translate(Mat4 const &m, Vec3 const &offset) noexcept {
        return glm::translate(m, offset);
    }

    /// Applies a rotation around axis to matrix m (angle in radians).
    [[nodiscard]] inline Mat4 Rotate(Mat4 const &m, float angle_radians, Vec3 const &axis) noexcept {
        return glm::rotate(m, angle_radians, axis);
    }

    /// Applies a scale to matrix m.
    [[nodiscard]] inline Mat4 Scale(Mat4 const &m, Vec3 const &factors) noexcept {
        return glm::scale(m, factors);
    }

    /// Returns the inverse of a matrix.
    [[nodiscard]] inline Mat4 Inverse(Mat4 const &m) noexcept {
        return glm::inverse(m);
    }

    /// Returns the transpose of a matrix.
    [[nodiscard]] inline Mat4 Transpose(Mat4 const &m) noexcept {
        return glm::transpose(m);
    }

    /// Creates a quaternion from an axis-angle pair (angle in radians).
    [[nodiscard]] inline Quat AngleAxis(float angle_radians, Vec3 const &axis) noexcept {
        return glm::angleAxis(angle_radians, axis);
    }

    /// Converts a quaternion to a 4x4 rotation matrix.
    [[nodiscard]] inline Mat4 QuatToMat4(Quat const &q) noexcept {
        return glm::mat4_cast(q);
    }

    /// Converts a quaternion to a 3x3 rotation matrix.
    [[nodiscard]] inline Mat3 QuatToMat3(Quat const &q) noexcept {
        return glm::mat3_cast(q);
    }

    /// Extracts a quaternion from a rotation matrix.
    [[nodiscard]] inline Quat Mat4ToQuat(Mat4 const &m) noexcept {
        return glm::quat_cast(m);
    }

    // =========================================================================
    // Camera / projection (Left-Handed, depth [0,1])
    //
    // NOTE: These use LH_ZO variants explicitly to match D3D/Vulkan conventions.
    //       GLM_FORCE_LEFT_HANDED and GLM_FORCE_DEPTH_ZERO_TO_ONE are also set
    //       globally, but explicit variants make the intent unambiguous.
    // =========================================================================

    /// Left-handed perspective projection with depth [0, 1].
    [[nodiscard]] inline Mat4 PerspectiveLH(float fov_y_radians, float aspect,
                                            float near_z, float far_z) noexcept {
        return glm::perspectiveLH_ZO(fov_y_radians, aspect, near_z, far_z);
    }

    /// Left-handed orthographic projection with depth [0, 1].
    [[nodiscard]] inline Mat4 OrthoLH(float left, float right,
                                      float bottom, float top,
                                      float near_z, float far_z) noexcept {
        return glm::orthoLH_ZO(left, right, bottom, top, near_z, far_z);
    }

    /// Left-handed look-at view matrix.
    [[nodiscard]] inline Mat4 LookAtLH(Vec3 const &eye, Vec3 const &target,
                                       Vec3 const &up) noexcept {
        return glm::lookAtLH(eye, target, up);
    }

    /// Returns a raw float pointer to a GLM type (for GPU uploads, etc.).
    [[nodiscard]] inline const float *ValuePtr(Vec2 const &v) noexcept { return glm::value_ptr(v); }
    [[nodiscard]] inline const float *ValuePtr(Vec3 const &v) noexcept { return glm::value_ptr(v); }
    [[nodiscard]] inline const float *ValuePtr(Vec4 const &v) noexcept { return glm::value_ptr(v); }
    [[nodiscard]] inline const float *ValuePtr(Mat3 const &m) noexcept { return glm::value_ptr(m); }
    [[nodiscard]] inline const float *ValuePtr(Mat4 const &m) noexcept { return glm::value_ptr(m); }
    [[nodiscard]] inline const float *ValuePtr(Quat const &q) noexcept { return glm::value_ptr(q); }

    // Mutable overloads
    [[nodiscard]] inline float *ValuePtr(Vec2 &v) noexcept { return glm::value_ptr(v); }
    [[nodiscard]] inline float *ValuePtr(Vec3 &v) noexcept { return glm::value_ptr(v); }
    [[nodiscard]] inline float *ValuePtr(Vec4 &v) noexcept { return glm::value_ptr(v); }
    [[nodiscard]] inline float *ValuePtr(Mat3 &m) noexcept { return glm::value_ptr(m); }
    [[nodiscard]] inline float *ValuePtr(Mat4 &m) noexcept { return glm::value_ptr(m); }
    [[nodiscard]] inline float *ValuePtr(Quat &q) noexcept { return glm::value_ptr(q); }

    /// Builds a TRS (Translate-Rotate-Scale) model matrix from components.
    [[nodiscard]] inline Mat4 ComposeTransform(Vec3 const &position,
                                               Quat const &rotation,
                                               Vec3 const &scale) noexcept {
        Mat4 m = Translate(Identity(), position);
        m = m * QuatToMat4(rotation);
        m = Scale(m, scale);
        return m;
    }
} // namespace CoreEngine::Math
