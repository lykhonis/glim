#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace glim {

struct Vec2 {
  float x = 0;
  float y = 0;

  Vec2() = default;
  constexpr Vec2(float x_, float y_) noexcept : x(x_), y(y_) {}
};

// Homogeneous 2D point: x and y over the projective weight z.
struct Vec3 {
  float x = 0;
  float y = 0;
  float z = 0;

  Vec3() = default;
  constexpr Vec3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}
};

struct Vec4 {
  float x = 0;
  float y = 0;
  float z = 0;
  float w = 0;

  Vec4() = default;
  constexpr Vec4(float x_, float y_, float z_, float w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
};

// Column-major: index = column * 4 + row.
struct Mat4 {
  float m[16]{};

  static Mat4 identity() noexcept;
  static Mat4 orthoYDown(
    float left, float top, float right, float bottom, float zNear = -1.0f, float zFar = 1.0f
  ) noexcept;
  static Mat4 translate(float x, float y, float z = 0) noexcept;
  static Mat4 scale(float x, float y, float z = 1) noexcept;
  static Mat4 rotateZ(float radians) noexcept;
  // Positive tips the bottom edge away from the viewer.
  static Mat4 rotateX(float radians) noexcept;
  // Positive swings the right edge away from the viewer.
  static Mat4 rotateY(float radians) noexcept;
  // Shears x by tan(x) per unit y and y by tan(y) per unit x.
  static Mat4 skew(float xRadians, float yRadians) noexcept;
  // CSS perspective: the eye sits distance px in front of the z = 0 plane.
  static Mat4 perspective(float distance) noexcept;

  Mat4 operator*(const Mat4& rhs) const noexcept;
  Vec4 operator*(const Vec4& v) const noexcept;

  bool is3D(float eps = 1e-5f) const noexcept;
};

bool isIdentity(const Mat4& m) noexcept;

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

// Column-major 3x3, for 2D projective maps (homographies).
struct Mat3 {
  float m[9]{};

  static Mat3 identity() noexcept;
  // Where t sends the z = 0 plane: (x, y, 1) to screen (x, y, w).
  static Mat3 planeOf(const Mat4& t) noexcept;
  Vec3 operator*(const Vec3& v) const noexcept;
  Mat3 operator*(const Mat3& rhs) const noexcept;
  std::optional<Mat3> inverse() const noexcept;
};

// A point of the z = 0 plane under t, before the divide: screen is (x/z, y/z),
// and z <= 0 means behind the eye.
Vec3 projectPlane(const Mat4& t, Vec2 p) noexcept;
// The plane point that t projects to screen; nullopt when the plane is edge-on
// or the point lies behind the eye.
std::optional<Vec2> unprojectPlane(const Mat4& t, Vec2 screen) noexcept;

// A linear alpha ramp between two points: fromAlpha at from, toAlpha at
// to, held beyond both.
struct Fade {
  Vec2 from{};
  Vec2 to{};
  float fromAlpha = 1.f;
  float toAlpha = 0.f;
};

// A Fade over a rect's unit square, as renderers and compositors apply it:
// the ramp parameter is dot(p - origin, step), clamped to [0, 1].
struct AlphaRamp {
  Vec2 origin{};
  Vec2 step{};
  float fromAlpha = 1.f;
  float toAlpha = 1.f;
  float at(Vec2 p) const noexcept;
  bool opaque() const noexcept { return fromAlpha >= 1.f && toAlpha >= 1.f; }
};

AlphaRamp rampOver(const Fade& fade, const Rect& rect) noexcept;

struct Color {
  std::uint32_t rgba = 0;

  Color() = default;
  explicit constexpr Color(std::uint32_t rgba_) noexcept : rgba(rgba_) {}
  constexpr Color(
    unsigned char r, unsigned char g, unsigned char b, unsigned char a = 0xff
  ) noexcept
      : rgba(
          (static_cast<std::uint32_t>(r) << 24) | (static_cast<std::uint32_t>(g) << 16) |
          (static_cast<std::uint32_t>(b) << 8) | a
        ) {}

  constexpr unsigned char r() const noexcept {
    return static_cast<unsigned char>((rgba >> 24) & 0xff);
  }
  constexpr unsigned char g() const noexcept {
    return static_cast<unsigned char>((rgba >> 16) & 0xff);
  }
  constexpr unsigned char b() const noexcept {
    return static_cast<unsigned char>((rgba >> 8) & 0xff);
  }
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
  constexpr Radius(float lt_, float rt_, float lb_, float rb_) noexcept
      : lt(lt_), rt(rt_), lb(lb_), rb(rb_) {}
};

inline bool nearlyEqual(float a, float b, float eps = 1e-5f) noexcept {
  return std::fabs(a - b) <= eps;
}

}  // namespace glim
