#pragma once

// =============================================================================
// CoreEngine::Math - engine-owned math value types and helpers.
//
// The core API exposes stable engine types instead of third-party math types.
// Keep backend-specific conversions inside platform/backend implementation files.
// =============================================================================

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace CoreEngine::Math {
    template<typename T>
    concept Scalar = std::same_as<T, float>
                     || std::same_as<T, double>
                     || std::same_as<T, int>
                     || std::same_as<T, unsigned int>;

    template<Scalar T>
    struct Vec2T {
        using value_type = T;

        T x{};
        T y{};

        constexpr Vec2T() noexcept = default;
        constexpr explicit Vec2T(T value) noexcept : x(value), y(value) {
        }
        constexpr Vec2T(T x_value, T y_value) noexcept : x(x_value), y(y_value) {
        }
    };

    template<Scalar T>
    struct Vec3T {
        using value_type = T;

        T x{};
        T y{};
        T z{};

        constexpr Vec3T() noexcept = default;
        constexpr explicit Vec3T(T value) noexcept : x(value), y(value), z(value) {
        }
        constexpr Vec3T(T x_value, T y_value, T z_value) noexcept
            : x(x_value), y(y_value), z(z_value) {
        }
    };

    template<Scalar T>
    struct Vec4T {
        using value_type = T;

        T x{};
        T y{};
        T z{};
        T w{};

        constexpr Vec4T() noexcept = default;
        constexpr explicit Vec4T(T value) noexcept : x(value), y(value), z(value), w(value) {
        }
        constexpr Vec4T(T x_value, T y_value, T z_value, T w_value) noexcept
            : x(x_value), y(y_value), z(z_value), w(w_value) {
        }
    };

    using Vec2 = Vec2T<float>;
    using Vec3 = Vec3T<float>;
    using Vec4 = Vec4T<float>;

    using IVec2 = Vec2T<int>;
    using IVec3 = Vec3T<int>;
    using IVec4 = Vec4T<int>;
    using UVec2 = Vec2T<unsigned int>;
    using UVec3 = Vec3T<unsigned int>;
    using UVec4 = Vec4T<unsigned int>;
    using DVec2 = Vec2T<double>;
    using DVec3 = Vec3T<double>;
    using DVec4 = Vec4T<double>;

    struct Mat2 {
        using value_type = float;

        float data[4]{};

        constexpr Mat2() noexcept : Mat2(1.f) {
        }
        constexpr explicit Mat2(float diagonal) noexcept {
            data[0] = diagonal;
            data[3] = diagonal;
        }
    };

    struct Mat3 {
        using value_type = float;

        float data[9]{};

        constexpr Mat3() noexcept : Mat3(1.f) {
        }
        constexpr explicit Mat3(float diagonal) noexcept {
            data[0] = diagonal;
            data[4] = diagonal;
            data[8] = diagonal;
        }
        constexpr Mat3(const Vec3 &column0, const Vec3 &column1, const Vec3 &column2) noexcept {
            SetColumn(0, column0);
            SetColumn(1, column1);
            SetColumn(2, column2);
        }

        [[nodiscard]] constexpr float &At(std::size_t row, std::size_t column) noexcept {
            return data[column * 3u + row];
        }

        [[nodiscard]] constexpr const float &At(std::size_t row, std::size_t column) const noexcept {
            return data[column * 3u + row];
        }

        constexpr void SetColumn(std::size_t column, const Vec3 &value) noexcept {
            At(0, column) = value.x;
            At(1, column) = value.y;
            At(2, column) = value.z;
        }
    };

    struct Mat4 {
        using value_type = float;

        float data[16]{};

        constexpr Mat4() noexcept : Mat4(1.f) {
        }
        constexpr explicit Mat4(float diagonal) noexcept {
            data[0] = diagonal;
            data[5] = diagonal;
            data[10] = diagonal;
            data[15] = diagonal;
        }
        constexpr explicit Mat4(const Mat3 &basis) noexcept : Mat4(1.f) {
            data[0] = basis.data[0];
            data[1] = basis.data[1];
            data[2] = basis.data[2];
            data[4] = basis.data[3];
            data[5] = basis.data[4];
            data[6] = basis.data[5];
            data[8] = basis.data[6];
            data[9] = basis.data[7];
            data[10] = basis.data[8];
        }

        [[nodiscard]] constexpr float &At(std::size_t row, std::size_t column) noexcept {
            return data[column * 4u + row];
        }

        [[nodiscard]] constexpr const float &At(std::size_t row, std::size_t column) const noexcept {
            return data[column * 4u + row];
        }
    };

    struct Quat {
        using value_type = float;

        float w{1.f};
        float x{};
        float y{};
        float z{};

        constexpr Quat() noexcept = default;
        constexpr Quat(float w_value, float x_value, float y_value, float z_value) noexcept
            : w(w_value), x(x_value), y(y_value), z(z_value) {
        }
    };

    template<typename T>
    concept Vector = std::same_as<std::remove_cvref_t<T>, Vec2>
                     || std::same_as<std::remove_cvref_t<T>, Vec3>
                     || std::same_as<std::remove_cvref_t<T>, Vec4>
                     || std::same_as<std::remove_cvref_t<T>, DVec2>
                     || std::same_as<std::remove_cvref_t<T>, DVec3>
                     || std::same_as<std::remove_cvref_t<T>, DVec4>
                     || std::same_as<std::remove_cvref_t<T>, IVec2>
                     || std::same_as<std::remove_cvref_t<T>, IVec3>
                     || std::same_as<std::remove_cvref_t<T>, IVec4>
                     || std::same_as<std::remove_cvref_t<T>, UVec2>
                     || std::same_as<std::remove_cvref_t<T>, UVec3>
                     || std::same_as<std::remove_cvref_t<T>, UVec4>;

    template<typename T>
    concept DotCompatible = Vector<T>;

    template<typename T>
    concept CrossCompatible = std::same_as<std::remove_cvref_t<T>, Vec3>
                              || std::same_as<std::remove_cvref_t<T>, DVec3>
                              || std::same_as<std::remove_cvref_t<T>, IVec3>;

    template<typename T>
    concept AngleConvertible = Scalar<T> || Vector<T>;

    inline constexpr float Pi      = 3.14159265358979323846f;
    inline constexpr float TwoPi   = 6.28318530717958647692f;
    inline constexpr float HalfPi  = 1.57079632679489661923f;
    inline constexpr float Epsilon = 1.192092896e-07f;

    template<Scalar T>
    [[nodiscard]] constexpr T Deg2Rad(T degrees) noexcept {
        return degrees * static_cast<T>(Pi / 180.f);
    }

    template<Scalar T>
    [[nodiscard]] constexpr T Rad2Deg(T radians) noexcept {
        return radians * static_cast<T>(180.f / Pi);
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec2T<T> Deg2Rad(const Vec2T<T> &degrees) noexcept {
        return {Deg2Rad(degrees.x), Deg2Rad(degrees.y)};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec3T<T> Deg2Rad(const Vec3T<T> &degrees) noexcept {
        return {Deg2Rad(degrees.x), Deg2Rad(degrees.y), Deg2Rad(degrees.z)};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec4T<T> Deg2Rad(const Vec4T<T> &degrees) noexcept {
        return {Deg2Rad(degrees.x), Deg2Rad(degrees.y), Deg2Rad(degrees.z), Deg2Rad(degrees.w)};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec2T<T> Rad2Deg(const Vec2T<T> &radians) noexcept {
        return {Rad2Deg(radians.x), Rad2Deg(radians.y)};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec3T<T> Rad2Deg(const Vec3T<T> &radians) noexcept {
        return {Rad2Deg(radians.x), Rad2Deg(radians.y), Rad2Deg(radians.z)};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec4T<T> Rad2Deg(const Vec4T<T> &radians) noexcept {
        return {Rad2Deg(radians.x), Rad2Deg(radians.y), Rad2Deg(radians.z), Rad2Deg(radians.w)};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec2T<T> operator+(const Vec2T<T> &a, const Vec2T<T> &b) noexcept {
        return {a.x + b.x, a.y + b.y};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec3T<T> operator+(const Vec3T<T> &a, const Vec3T<T> &b) noexcept {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec4T<T> operator+(const Vec4T<T> &a, const Vec4T<T> &b) noexcept {
        return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec2T<T> operator-(const Vec2T<T> &a, const Vec2T<T> &b) noexcept {
        return {a.x - b.x, a.y - b.y};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec3T<T> operator-(const Vec3T<T> &a, const Vec3T<T> &b) noexcept {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec4T<T> operator-(const Vec4T<T> &a, const Vec4T<T> &b) noexcept {
        return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec2T<T> operator-(const Vec2T<T> &v) noexcept {
        return {-v.x, -v.y};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec3T<T> operator-(const Vec3T<T> &v) noexcept {
        return {-v.x, -v.y, -v.z};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec4T<T> operator-(const Vec4T<T> &v) noexcept {
        return {-v.x, -v.y, -v.z, -v.w};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec2T<T> operator*(const Vec2T<T> &v, T scalar) noexcept {
        return {v.x * scalar, v.y * scalar};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec3T<T> operator*(const Vec3T<T> &v, T scalar) noexcept {
        return {v.x * scalar, v.y * scalar, v.z * scalar};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec4T<T> operator*(const Vec4T<T> &v, T scalar) noexcept {
        return {v.x * scalar, v.y * scalar, v.z * scalar, v.w * scalar};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec2T<T> operator*(T scalar, const Vec2T<T> &v) noexcept {
        return v * scalar;
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec3T<T> operator*(T scalar, const Vec3T<T> &v) noexcept {
        return v * scalar;
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec4T<T> operator*(T scalar, const Vec4T<T> &v) noexcept {
        return v * scalar;
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec2T<T> operator/(const Vec2T<T> &v, T scalar) noexcept {
        return {v.x / scalar, v.y / scalar};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec3T<T> operator/(const Vec3T<T> &v, T scalar) noexcept {
        return {v.x / scalar, v.y / scalar, v.z / scalar};
    }

    template<Scalar T>
    [[nodiscard]] constexpr Vec4T<T> operator/(const Vec4T<T> &v, T scalar) noexcept {
        return {v.x / scalar, v.y / scalar, v.z / scalar, v.w / scalar};
    }

    template<Scalar T>
    constexpr Vec2T<T> &operator+=(Vec2T<T> &a, const Vec2T<T> &b) noexcept {
        a = a + b;
        return a;
    }

    template<Scalar T>
    constexpr Vec3T<T> &operator+=(Vec3T<T> &a, const Vec3T<T> &b) noexcept {
        a = a + b;
        return a;
    }

    template<Scalar T>
    constexpr Vec4T<T> &operator+=(Vec4T<T> &a, const Vec4T<T> &b) noexcept {
        a = a + b;
        return a;
    }

    [[nodiscard]] constexpr Mat4 operator*(const Mat4 &a, const Mat4 &b) noexcept {
        Mat4 result{0.f};
        result.data[0] = a.data[0] * b.data[0] + a.data[4] * b.data[1] + a.data[8] * b.data[2] + a.data[12] * b.data[3];
        result.data[1] = a.data[1] * b.data[0] + a.data[5] * b.data[1] + a.data[9] * b.data[2] + a.data[13] * b.data[3];
        result.data[2] = a.data[2] * b.data[0] + a.data[6] * b.data[1] + a.data[10] * b.data[2] + a.data[14] * b.data[3];
        result.data[3] = a.data[3] * b.data[0] + a.data[7] * b.data[1] + a.data[11] * b.data[2] + a.data[15] * b.data[3];

        result.data[4] = a.data[0] * b.data[4] + a.data[4] * b.data[5] + a.data[8] * b.data[6] + a.data[12] * b.data[7];
        result.data[5] = a.data[1] * b.data[4] + a.data[5] * b.data[5] + a.data[9] * b.data[6] + a.data[13] * b.data[7];
        result.data[6] = a.data[2] * b.data[4] + a.data[6] * b.data[5] + a.data[10] * b.data[6] + a.data[14] * b.data[7];
        result.data[7] = a.data[3] * b.data[4] + a.data[7] * b.data[5] + a.data[11] * b.data[6] + a.data[15] * b.data[7];

        result.data[8] = a.data[0] * b.data[8] + a.data[4] * b.data[9] + a.data[8] * b.data[10] + a.data[12] * b.data[11];
        result.data[9] = a.data[1] * b.data[8] + a.data[5] * b.data[9] + a.data[9] * b.data[10] + a.data[13] * b.data[11];
        result.data[10] = a.data[2] * b.data[8] + a.data[6] * b.data[9] + a.data[10] * b.data[10] + a.data[14] * b.data[11];
        result.data[11] = a.data[3] * b.data[8] + a.data[7] * b.data[9] + a.data[11] * b.data[10] + a.data[15] * b.data[11];

        result.data[12] = a.data[0] * b.data[12] + a.data[4] * b.data[13] + a.data[8] * b.data[14] + a.data[12] * b.data[15];
        result.data[13] = a.data[1] * b.data[12] + a.data[5] * b.data[13] + a.data[9] * b.data[14] + a.data[13] * b.data[15];
        result.data[14] = a.data[2] * b.data[12] + a.data[6] * b.data[13] + a.data[10] * b.data[14] + a.data[14] * b.data[15];
        result.data[15] = a.data[3] * b.data[12] + a.data[7] * b.data[13] + a.data[11] * b.data[14] + a.data[15] * b.data[15];
        return result;
    }

    template<DotCompatible T>
    [[nodiscard]] constexpr auto Dot(const T &a, const T &b) noexcept -> typename T::value_type {
        if constexpr (requires { a.w; }) {
            return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        } else if constexpr (requires { a.z; }) {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        } else {
            return a.x * b.x + a.y * b.y;
        }
    }

    template<CrossCompatible T>
    [[nodiscard]] constexpr T Cross(const T &a, const T &b) noexcept {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x,
        };
    }

    template<DotCompatible T>
    [[nodiscard]] inline T Normalize(const T &v) noexcept {
        const auto length_squared = Dot(v, v);
        if (length_squared <= static_cast<typename T::value_type>(0)) {
            return T{};
        }

        const auto inv_length = static_cast<typename T::value_type>(1) / std::sqrt(length_squared);
        return v * inv_length;
    }

    template<DotCompatible T>
    [[nodiscard]] inline auto Length(const T &v) noexcept -> typename T::value_type {
        return std::sqrt(Dot(v, v));
    }

    template<DotCompatible T>
    [[nodiscard]] constexpr auto LengthSquared(const T &v) noexcept -> typename T::value_type {
        return Dot(v, v);
    }

    template<DotCompatible T>
    [[nodiscard]] inline auto Distance(const T &a, const T &b) noexcept -> typename T::value_type {
        return Length(a - b);
    }

    template<DotCompatible T>
    [[nodiscard]] constexpr T Reflect(const T &incident, const T &normal) noexcept {
        return incident - normal * (static_cast<typename T::value_type>(2) * Dot(normal, incident));
    }

    template<typename T>
    [[nodiscard]] constexpr T Lerp(const T &a, const T &b, float t) noexcept {
        return a + (b - a) * t;
    }

    [[nodiscard]] inline Quat Normalize(const Quat &q) noexcept {
        const float length_squared = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
        if (length_squared <= 0.f) {
            return {};
        }

        const float inv_length = 1.f / std::sqrt(length_squared);
        return {q.w * inv_length, q.x * inv_length, q.y * inv_length, q.z * inv_length};
    }

    [[nodiscard]] inline Quat Slerp(const Quat &a, const Quat &b, float t) noexcept {
        Quat end = b;
        float cos_theta = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;

        if (cos_theta < 0.f) {
            end = {-b.w, -b.x, -b.y, -b.z};
            cos_theta = -cos_theta;
        }

        if (cos_theta > 0.9995f) {
            return Normalize(Quat{
                a.w + (end.w - a.w) * t,
                a.x + (end.x - a.x) * t,
                a.y + (end.y - a.y) * t,
                a.z + (end.z - a.z) * t,
            });
        }

        const float theta = std::acos(std::clamp(cos_theta, -1.f, 1.f));
        const float sin_theta = std::sin(theta);
        const float weight_a = std::sin((1.f - t) * theta) / sin_theta;
        const float weight_b = std::sin(t * theta) / sin_theta;

        return {
            a.w * weight_a + end.w * weight_b,
            a.x * weight_a + end.x * weight_b,
            a.y * weight_a + end.y * weight_b,
            a.z * weight_a + end.z * weight_b,
        };
    }

    [[nodiscard]] inline float SmoothStep(float edge0, float edge1, float x) noexcept {
        const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    template<typename T>
        requires std::totally_ordered<T>
    [[nodiscard]] constexpr T Clamp(const T &value, const T &min_val, const T &max_val) noexcept {
        return std::clamp(value, min_val, max_val);
    }

    template<typename T>
        requires std::totally_ordered<T>
    [[nodiscard]] constexpr T Min(const T &a, const T &b) noexcept {
        return std::min(a, b);
    }

    template<typename T>
        requires std::totally_ordered<T>
    [[nodiscard]] constexpr T Max(const T &a, const T &b) noexcept {
        return std::max(a, b);
    }

    template<typename T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] constexpr T Abs(T value) noexcept {
        return value < static_cast<T>(0) ? -value : value;
    }

    [[nodiscard]] constexpr Mat4 Identity() noexcept {
        return Mat4(1.f);
    }

    [[nodiscard]] constexpr Mat4 Translate(const Mat4 &m, const Vec3 &offset) noexcept {
        Mat4 result = m;
        result.data[12] = m.data[0] * offset.x + m.data[4] * offset.y + m.data[8] * offset.z + m.data[12];
        result.data[13] = m.data[1] * offset.x + m.data[5] * offset.y + m.data[9] * offset.z + m.data[13];
        result.data[14] = m.data[2] * offset.x + m.data[6] * offset.y + m.data[10] * offset.z + m.data[14];
        result.data[15] = m.data[3] * offset.x + m.data[7] * offset.y + m.data[11] * offset.z + m.data[15];
        return result;
    }

    [[nodiscard]] inline Mat4 Rotate(const Mat4 &m, float angle_radians, const Vec3 &axis) noexcept {
        const float axis_length_squared = Dot(axis, axis);
        if (axis_length_squared <= 0.f) {
            return m;
        }

        const float inv_axis_length = 1.f / std::sqrt(axis_length_squared);
        const Vec3 n = axis * inv_axis_length;
        const float c = std::cos(angle_radians);
        const float s = std::sin(angle_radians);
        const float t = 1.f - c;

        const float r00 = t * n.x * n.x + c;
        const float r01 = t * n.x * n.y - s * n.z;
        const float r02 = t * n.x * n.z + s * n.y;
        const float r10 = t * n.x * n.y + s * n.z;
        const float r11 = t * n.y * n.y + c;
        const float r12 = t * n.y * n.z - s * n.x;
        const float r20 = t * n.x * n.z - s * n.y;
        const float r21 = t * n.y * n.z + s * n.x;
        const float r22 = t * n.z * n.z + c;

        Mat4 result = m;
        result.data[0] = m.data[0] * r00 + m.data[4] * r10 + m.data[8] * r20;
        result.data[1] = m.data[1] * r00 + m.data[5] * r10 + m.data[9] * r20;
        result.data[2] = m.data[2] * r00 + m.data[6] * r10 + m.data[10] * r20;
        result.data[3] = m.data[3] * r00 + m.data[7] * r10 + m.data[11] * r20;

        result.data[4] = m.data[0] * r01 + m.data[4] * r11 + m.data[8] * r21;
        result.data[5] = m.data[1] * r01 + m.data[5] * r11 + m.data[9] * r21;
        result.data[6] = m.data[2] * r01 + m.data[6] * r11 + m.data[10] * r21;
        result.data[7] = m.data[3] * r01 + m.data[7] * r11 + m.data[11] * r21;

        result.data[8] = m.data[0] * r02 + m.data[4] * r12 + m.data[8] * r22;
        result.data[9] = m.data[1] * r02 + m.data[5] * r12 + m.data[9] * r22;
        result.data[10] = m.data[2] * r02 + m.data[6] * r12 + m.data[10] * r22;
        result.data[11] = m.data[3] * r02 + m.data[7] * r12 + m.data[11] * r22;
        return result;
    }

    [[nodiscard]] constexpr Mat4 Scale(const Mat4 &m, const Vec3 &factors) noexcept {
        Mat4 result = m;
        result.data[0] *= factors.x;
        result.data[1] *= factors.x;
        result.data[2] *= factors.x;
        result.data[3] *= factors.x;
        result.data[4] *= factors.y;
        result.data[5] *= factors.y;
        result.data[6] *= factors.y;
        result.data[7] *= factors.y;
        result.data[8] *= factors.z;
        result.data[9] *= factors.z;
        result.data[10] *= factors.z;
        result.data[11] *= factors.z;
        return result;
    }

    [[nodiscard]] inline Mat4 Inverse(const Mat4 &m) noexcept {
        const float *a = m.data;
        Mat4 inv{0.f};

        inv.data[0] = a[5] * a[10] * a[15] -
                      a[5] * a[11] * a[14] -
                      a[9] * a[6] * a[15] +
                      a[9] * a[7] * a[14] +
                      a[13] * a[6] * a[11] -
                      a[13] * a[7] * a[10];

        inv.data[4] = -a[4] * a[10] * a[15] +
                      a[4] * a[11] * a[14] +
                      a[8] * a[6] * a[15] -
                      a[8] * a[7] * a[14] -
                      a[12] * a[6] * a[11] +
                      a[12] * a[7] * a[10];

        inv.data[8] = a[4] * a[9] * a[15] -
                      a[4] * a[11] * a[13] -
                      a[8] * a[5] * a[15] +
                      a[8] * a[7] * a[13] +
                      a[12] * a[5] * a[11] -
                      a[12] * a[7] * a[9];

        inv.data[12] = -a[4] * a[9] * a[14] +
                       a[4] * a[10] * a[13] +
                       a[8] * a[5] * a[14] -
                       a[8] * a[6] * a[13] -
                       a[12] * a[5] * a[10] +
                       a[12] * a[6] * a[9];

        inv.data[1] = -a[1] * a[10] * a[15] +
                      a[1] * a[11] * a[14] +
                      a[9] * a[2] * a[15] -
                      a[9] * a[3] * a[14] -
                      a[13] * a[2] * a[11] +
                      a[13] * a[3] * a[10];

        inv.data[5] = a[0] * a[10] * a[15] -
                      a[0] * a[11] * a[14] -
                      a[8] * a[2] * a[15] +
                      a[8] * a[3] * a[14] +
                      a[12] * a[2] * a[11] -
                      a[12] * a[3] * a[10];

        inv.data[9] = -a[0] * a[9] * a[15] +
                      a[0] * a[11] * a[13] +
                      a[8] * a[1] * a[15] -
                      a[8] * a[3] * a[13] -
                      a[12] * a[1] * a[11] +
                      a[12] * a[3] * a[9];

        inv.data[13] = a[0] * a[9] * a[14] -
                       a[0] * a[10] * a[13] -
                       a[8] * a[1] * a[14] +
                       a[8] * a[2] * a[13] +
                       a[12] * a[1] * a[10] -
                       a[12] * a[2] * a[9];

        inv.data[2] = a[1] * a[6] * a[15] -
                      a[1] * a[7] * a[14] -
                      a[5] * a[2] * a[15] +
                      a[5] * a[3] * a[14] +
                      a[13] * a[2] * a[7] -
                      a[13] * a[3] * a[6];

        inv.data[6] = -a[0] * a[6] * a[15] +
                      a[0] * a[7] * a[14] +
                      a[4] * a[2] * a[15] -
                      a[4] * a[3] * a[14] -
                      a[12] * a[2] * a[7] +
                      a[12] * a[3] * a[6];

        inv.data[10] = a[0] * a[5] * a[15] -
                       a[0] * a[7] * a[13] -
                       a[4] * a[1] * a[15] +
                       a[4] * a[3] * a[13] +
                       a[12] * a[1] * a[7] -
                       a[12] * a[3] * a[5];

        inv.data[14] = -a[0] * a[5] * a[14] +
                       a[0] * a[6] * a[13] +
                       a[4] * a[1] * a[14] -
                       a[4] * a[2] * a[13] -
                       a[12] * a[1] * a[6] +
                       a[12] * a[2] * a[5];

        inv.data[3] = -a[1] * a[6] * a[11] +
                      a[1] * a[7] * a[10] +
                      a[5] * a[2] * a[11] -
                      a[5] * a[3] * a[10] -
                      a[9] * a[2] * a[7] +
                      a[9] * a[3] * a[6];

        inv.data[7] = a[0] * a[6] * a[11] -
                      a[0] * a[7] * a[10] -
                      a[4] * a[2] * a[11] +
                      a[4] * a[3] * a[10] +
                      a[8] * a[2] * a[7] -
                      a[8] * a[3] * a[6];

        inv.data[11] = -a[0] * a[5] * a[11] +
                       a[0] * a[7] * a[9] +
                       a[4] * a[1] * a[11] -
                       a[4] * a[3] * a[9] -
                       a[8] * a[1] * a[7] +
                       a[8] * a[3] * a[5];

        inv.data[15] = a[0] * a[5] * a[10] -
                       a[0] * a[6] * a[9] -
                       a[4] * a[1] * a[10] +
                       a[4] * a[2] * a[9] +
                       a[8] * a[1] * a[6] -
                       a[8] * a[2] * a[5];

        const float determinant = a[0] * inv.data[0] + a[1] * inv.data[4] +
                                  a[2] * inv.data[8] + a[3] * inv.data[12];
        if (std::abs(determinant) <= Epsilon) {
            return Mat4{1.f};
        }

        const float inverse_determinant = 1.f / determinant;
        for (float &value: inv.data) {
            value *= inverse_determinant;
        }

        return inv;
    }

    [[nodiscard]] constexpr Mat4 Transpose(const Mat4 &m) noexcept {
        Mat4 result{0.f};
        result.data[0] = m.data[0];
        result.data[1] = m.data[4];
        result.data[2] = m.data[8];
        result.data[3] = m.data[12];
        result.data[4] = m.data[1];
        result.data[5] = m.data[5];
        result.data[6] = m.data[9];
        result.data[7] = m.data[13];
        result.data[8] = m.data[2];
        result.data[9] = m.data[6];
        result.data[10] = m.data[10];
        result.data[11] = m.data[14];
        result.data[12] = m.data[3];
        result.data[13] = m.data[7];
        result.data[14] = m.data[11];
        result.data[15] = m.data[15];
        return result;
    }

    [[nodiscard]] inline Quat AngleAxis(float angle_radians, const Vec3 &axis) noexcept {
        const float axis_length_squared = Dot(axis, axis);
        if (axis_length_squared <= 0.f) {
            return {};
        }

        const float inv_axis_length = 1.f / std::sqrt(axis_length_squared);
        const Vec3 n = axis * inv_axis_length;
        const float half_angle = angle_radians * 0.5f;
        const float s = std::sin(half_angle);
        return {std::cos(half_angle), n.x * s, n.y * s, n.z * s};
    }

    [[nodiscard]] constexpr Mat3 QuatToMat3(const Quat &q) noexcept {
        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;
        const float xy = q.x * q.y;
        const float xz = q.x * q.z;
        const float yz = q.y * q.z;
        const float wx = q.w * q.x;
        const float wy = q.w * q.y;
        const float wz = q.w * q.z;

        Mat3 result{1.f};
        result.At(0, 0) = 1.f - 2.f * (yy + zz);
        result.At(0, 1) = 2.f * (xy - wz);
        result.At(0, 2) = 2.f * (xz + wy);
        result.At(1, 0) = 2.f * (xy + wz);
        result.At(1, 1) = 1.f - 2.f * (xx + zz);
        result.At(1, 2) = 2.f * (yz - wx);
        result.At(2, 0) = 2.f * (xz - wy);
        result.At(2, 1) = 2.f * (yz + wx);
        result.At(2, 2) = 1.f - 2.f * (xx + yy);
        return result;
    }

    [[nodiscard]] constexpr Mat4 QuatToMat4(const Quat &q) noexcept {
        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;
        const float xy = q.x * q.y;
        const float xz = q.x * q.z;
        const float yz = q.y * q.z;
        const float wx = q.w * q.x;
        const float wy = q.w * q.y;
        const float wz = q.w * q.z;

        Mat4 result{1.f};
        result.data[0] = 1.f - 2.f * (yy + zz);
        result.data[1] = 2.f * (xy + wz);
        result.data[2] = 2.f * (xz - wy);
        result.data[4] = 2.f * (xy - wz);
        result.data[5] = 1.f - 2.f * (xx + zz);
        result.data[6] = 2.f * (yz + wx);
        result.data[8] = 2.f * (xz + wy);
        result.data[9] = 2.f * (yz - wx);
        result.data[10] = 1.f - 2.f * (xx + yy);
        return result;
    }

    [[nodiscard]] inline Quat Mat4ToQuat(const Mat4 &m) noexcept {
        const float trace = m.At(0, 0) + m.At(1, 1) + m.At(2, 2);
        Quat q;

        if (trace > 0.f) {
            const float s = std::sqrt(trace + 1.f) * 2.f;
            q.w = 0.25f * s;
            q.x = (m.At(2, 1) - m.At(1, 2)) / s;
            q.y = (m.At(0, 2) - m.At(2, 0)) / s;
            q.z = (m.At(1, 0) - m.At(0, 1)) / s;
        } else if (m.At(0, 0) > m.At(1, 1) && m.At(0, 0) > m.At(2, 2)) {
            const float s = std::sqrt(1.f + m.At(0, 0) - m.At(1, 1) - m.At(2, 2)) * 2.f;
            q.w = (m.At(2, 1) - m.At(1, 2)) / s;
            q.x = 0.25f * s;
            q.y = (m.At(0, 1) + m.At(1, 0)) / s;
            q.z = (m.At(0, 2) + m.At(2, 0)) / s;
        } else if (m.At(1, 1) > m.At(2, 2)) {
            const float s = std::sqrt(1.f + m.At(1, 1) - m.At(0, 0) - m.At(2, 2)) * 2.f;
            q.w = (m.At(0, 2) - m.At(2, 0)) / s;
            q.x = (m.At(0, 1) + m.At(1, 0)) / s;
            q.y = 0.25f * s;
            q.z = (m.At(1, 2) + m.At(2, 1)) / s;
        } else {
            const float s = std::sqrt(1.f + m.At(2, 2) - m.At(0, 0) - m.At(1, 1)) * 2.f;
            q.w = (m.At(1, 0) - m.At(0, 1)) / s;
            q.x = (m.At(0, 2) + m.At(2, 0)) / s;
            q.y = (m.At(1, 2) + m.At(2, 1)) / s;
            q.z = 0.25f * s;
        }

        return Normalize(q);
    }

    [[nodiscard]] inline Vec3 operator*(const Quat &q, const Vec3 &v) noexcept {
        const Vec3 qv{q.x, q.y, q.z};
        const Vec3 uv = Cross(qv, v);
        const Vec3 uuv = Cross(qv, uv);
        return v + ((uv * q.w) + uuv) * 2.f;
    }

    [[nodiscard]] inline Mat4 PerspectiveLH(float fov_y_radians, float aspect,
                                            float near_z, float far_z) noexcept {
        const float tan_half_fovy = std::tan(fov_y_radians * 0.5f);
        Mat4 result{0.f};
        result.At(0, 0) = 1.f / (aspect * tan_half_fovy);
        result.At(1, 1) = 1.f / tan_half_fovy;
        result.At(2, 2) = far_z / (far_z - near_z);
        result.At(3, 2) = 1.f;
        result.At(2, 3) = -(far_z * near_z) / (far_z - near_z);
        return result;
    }

    [[nodiscard]] constexpr Mat4 OrthoLH(float left, float right,
                                         float bottom, float top,
                                         float near_z, float far_z) noexcept {
        Mat4 result{1.f};
        result.At(0, 0) = 2.f / (right - left);
        result.At(1, 1) = 2.f / (top - bottom);
        result.At(2, 2) = 1.f / (far_z - near_z);
        result.At(0, 3) = -(right + left) / (right - left);
        result.At(1, 3) = -(top + bottom) / (top - bottom);
        result.At(2, 3) = -near_z / (far_z - near_z);
        return result;
    }

    [[nodiscard]] inline Mat4 LookAtLH(const Vec3 &eye, const Vec3 &target,
                                       const Vec3 &up) noexcept {
        const Vec3 forward = Normalize(target - eye);
        const Vec3 right = Normalize(Cross(up, forward));
        const Vec3 camera_up = Cross(forward, right);

        Mat4 result{1.f};
        result.At(0, 0) = right.x;
        result.At(0, 1) = right.y;
        result.At(0, 2) = right.z;
        result.At(1, 0) = camera_up.x;
        result.At(1, 1) = camera_up.y;
        result.At(1, 2) = camera_up.z;
        result.At(2, 0) = forward.x;
        result.At(2, 1) = forward.y;
        result.At(2, 2) = forward.z;
        result.At(0, 3) = -Dot(right, eye);
        result.At(1, 3) = -Dot(camera_up, eye);
        result.At(2, 3) = -Dot(forward, eye);
        return result;
    }

    [[nodiscard]] constexpr const float *ValuePtr(const Vec2 &v) noexcept { return &v.x; }
    [[nodiscard]] constexpr const float *ValuePtr(const Vec3 &v) noexcept { return &v.x; }
    [[nodiscard]] constexpr const float *ValuePtr(const Vec4 &v) noexcept { return &v.x; }
    [[nodiscard]] constexpr const float *ValuePtr(const Mat3 &m) noexcept { return m.data; }
    [[nodiscard]] constexpr const float *ValuePtr(const Mat4 &m) noexcept { return m.data; }
    [[nodiscard]] constexpr const float *ValuePtr(const Quat &q) noexcept { return &q.w; }

    [[nodiscard]] constexpr float *ValuePtr(Vec2 &v) noexcept { return &v.x; }
    [[nodiscard]] constexpr float *ValuePtr(Vec3 &v) noexcept { return &v.x; }
    [[nodiscard]] constexpr float *ValuePtr(Vec4 &v) noexcept { return &v.x; }
    [[nodiscard]] constexpr float *ValuePtr(Mat3 &m) noexcept { return m.data; }
    [[nodiscard]] constexpr float *ValuePtr(Mat4 &m) noexcept { return m.data; }
    [[nodiscard]] constexpr float *ValuePtr(Quat &q) noexcept { return &q.w; }

    [[nodiscard]] constexpr Mat4 ComposeTransform(const Vec3 &position,
                                                  const Quat &rotation,
                                                  const Vec3 &scale) noexcept {
        const float xx = rotation.x * rotation.x;
        const float yy = rotation.y * rotation.y;
        const float zz = rotation.z * rotation.z;
        const float xy = rotation.x * rotation.y;
        const float xz = rotation.x * rotation.z;
        const float yz = rotation.y * rotation.z;
        const float wx = rotation.w * rotation.x;
        const float wy = rotation.w * rotation.y;
        const float wz = rotation.w * rotation.z;

        Mat4 result{1.f};
        result.data[0] = (1.f - 2.f * (yy + zz)) * scale.x;
        result.data[1] = (2.f * (xy + wz)) * scale.x;
        result.data[2] = (2.f * (xz - wy)) * scale.x;
        result.data[4] = (2.f * (xy - wz)) * scale.y;
        result.data[5] = (1.f - 2.f * (xx + zz)) * scale.y;
        result.data[6] = (2.f * (yz + wx)) * scale.y;
        result.data[8] = (2.f * (xz + wy)) * scale.z;
        result.data[9] = (2.f * (yz - wx)) * scale.z;
        result.data[10] = (1.f - 2.f * (xx + yy)) * scale.z;
        result.data[12] = position.x;
        result.data[13] = position.y;
        result.data[14] = position.z;
        return result;
    }

    static_assert(sizeof(Vec2) == sizeof(float) * 2u);
    static_assert(sizeof(Vec3) == sizeof(float) * 3u);
    static_assert(sizeof(Vec4) == sizeof(float) * 4u);
    static_assert(sizeof(Mat4) == sizeof(float) * 16u);
} // namespace CoreEngine::Math
