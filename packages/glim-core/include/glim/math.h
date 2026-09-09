#pragma once

#include <cstdint>
#include <cmath>

namespace glim {

struct Vec2 {
    float x = 0;
    float y = 0;

    Vec2() = default;
    constexpr Vec2(float x_, float y_) noexcept : x(x_), y(y_) {}
};

struct Vec4 {
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 0;

    Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
};

// Column-major 4x4. Index: column * 4 + row (m[0] is column 0 row 0).
struct Mat4 {
    float m[16]{};

    static Mat4 identity() noexcept;
    static Mat4 orthoYDown(float left, float top, float right, float bottom,
                           float zNear = -1.0f, float zFar = 1.0f) noexcept;
    static Mat4 translate(float x, float y, float z = 0) noexcept;
    static Mat4 scale(float x, float y, float z = 1) noexcept;

    Mat4 operator*(const Mat4& rhs) const noexcept;
    Vec4 operator*(const Vec4& v) const noexcept;

    bool is3D(float eps = 1e-5f) const noexcept;
};

struct Rect {
    Vec2 origin;
    Vec2 size;

    Rect() = default;
    constexpr Rect(Vec2 origin_, Vec2 size_) noexcept : origin(origin_), size(size_) {}

    static constexpr Rect fromSize(Vec2 size) noexcept { return Rect{{0, 0}, size}; }
    static constexpr Rect fromSize(float w, float h) noexcept { return fromSize(Vec2{w, h}); }

    constexpr float x() const noexcept { return origin.x; }
    constexpr float y() const noexcept { return origin.y; }
    constexpr float width() const noexcept { return size.x; }
    constexpr float height() const noexcept { return size.y; }
};

struct Color {
    std::uint32_t rgba = 0;

    Color() = default;
    explicit constexpr Color(std::uint32_t rgba_) noexcept : rgba(rgba_) {}
    constexpr Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 0xff) noexcept
        : rgba((static_cast<std::uint32_t>(r) << 24) | (static_cast<std::uint32_t>(g) << 16) |
               (static_cast<std::uint32_t>(b) << 8) | a) {}

    constexpr unsigned char r() const noexcept { return static_cast<unsigned char>((rgba >> 24) & 0xff); }
    constexpr unsigned char g() const noexcept { return static_cast<unsigned char>((rgba >> 16) & 0xff); }
    constexpr unsigned char b() const noexcept { return static_cast<unsigned char>((rgba >> 8) & 0xff); }
    constexpr unsigned char a() const noexcept { return static_cast<unsigned char>(rgba & 0xff); }

    constexpr float red() const noexcept { return static_cast<float>(r()) / 255.0f; }
    constexpr float green() const noexcept { return static_cast<float>(g()) / 255.0f; }
    constexpr float blue() const noexcept { return static_cast<float>(b()) / 255.0f; }
    constexpr float alpha() const noexcept { return static_cast<float>(a()) / 255.0f; }

    Vec4 premul() const noexcept {
        const float al = alpha();
        return {red() * al, green() * al, blue() * al, al};
    }
};

struct Radius {
    float lt = 0;
    float rt = 0;
    float lb = 0;
    float rb = 0;

    Radius() = default;
    explicit constexpr Radius(float all) noexcept : lt(all), rt(all), lb(all), rb(all) {}
    constexpr Radius(float lt_, float rt_, float lb_, float rb_) noexcept : lt(lt_), rt(rt_), lb(lb_), rb(rb_) {}
};

inline bool nearlyEqual(float a, float b, float eps = 1e-5f) noexcept {
    return std::fabs(a - b) <= eps;
}

}  // namespace glim
