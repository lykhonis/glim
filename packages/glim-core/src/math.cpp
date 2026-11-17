#include <glim/math.h>

namespace glim {

Mat4 Mat4::identity() noexcept {
  Mat4 r{};
  r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
  return r;
}

Mat4 Mat4::orthoYDown(
  float left, float top, float right, float bottom, float zNear, float zFar
) noexcept {
  // Y-down paint to Y-up NDC; equiv. glm::ortho(l, r, b, t, zn, zf).
  const float rl = right - left;
  const float tb = top - bottom;
  const float fn = zFar - zNear;
  Mat4 r = identity();
  r.m[0] = 2.0f / rl;
  r.m[5] = 2.0f / tb;
  r.m[10] = -2.0f / fn;
  r.m[12] = -(right + left) / rl;
  r.m[13] = -(top + bottom) / tb;
  r.m[14] = -(zFar + zNear) / fn;
  return r;
}

Mat4 Mat4::translate(float x, float y, float z) noexcept {
  Mat4 r = identity();
  r.m[12] = x;
  r.m[13] = y;
  r.m[14] = z;
  return r;
}

Mat4 Mat4::scale(float x, float y, float z) noexcept {
  Mat4 r = identity();
  r.m[0] = x;
  r.m[5] = y;
  r.m[10] = z;
  return r;
}

Mat4 Mat4::rotateZ(float radians) noexcept {
  // Column-major CCW rotation about +Z (Y-down paint: positive angle
  // reads clockwise on screen, matching compass convention).
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  Mat4 r = identity();
  r.m[0] = c;
  r.m[1] = s;
  r.m[4] = -s;
  r.m[5] = c;
  return r;
}

Mat4 Mat4::operator*(const Mat4& rhs) const noexcept {
  Mat4 out{};
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      float s = 0;
      for (int k = 0; k < 4; ++k) {
        s += m[k * 4 + row] * rhs.m[col * 4 + k];
      }
      out.m[col * 4 + row] = s;
    }
  }
  return out;
}

Vec4 Mat4::operator*(const Vec4& v) const noexcept {
  return {
    m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
    m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
    m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
    m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w,
  };
}

Mat4 Mat4::rotateX(float radians) noexcept {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  Mat4 r = identity();
  r.m[5] = c;
  r.m[6] = s;
  r.m[9] = -s;
  r.m[10] = c;
  return r;
}

Mat4 Mat4::rotateY(float radians) noexcept {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  Mat4 r = identity();
  r.m[0] = c;
  r.m[2] = -s;
  r.m[8] = s;
  r.m[10] = c;
  return r;
}

Mat4 Mat4::perspective(float distance) noexcept {
  Mat4 r = identity();
  if (distance > 0) r.m[11] = -1.0f / distance;
  return r;
}

Mat3 Mat3::identity() noexcept {
  Mat3 r;
  r.m[0] = r.m[4] = r.m[8] = 1;
  return r;
}

Mat3 Mat3::planeOf(const Mat4& t) noexcept {
  Mat3 r;
  const int rows[3] = {0, 1, 3};
  const int cols[3] = {0, 1, 3};
  for (int c = 0; c < 3; ++c) {
    for (int k = 0; k < 3; ++k) r.m[c * 3 + k] = t.m[cols[c] * 4 + rows[k]];
  }
  return r;
}

Vec3 Mat3::operator*(const Vec3& v) const noexcept {
  return {
    m[0] * v.x + m[3] * v.y + m[6] * v.z,
    m[1] * v.x + m[4] * v.y + m[7] * v.z,
    m[2] * v.x + m[5] * v.y + m[8] * v.z,
  };
}

Mat3 Mat3::operator*(const Mat3& rhs) const noexcept {
  Mat3 r;
  for (int c = 0; c < 3; ++c) {
    const Vec3 col = *this * Vec3{rhs.m[c * 3], rhs.m[c * 3 + 1], rhs.m[c * 3 + 2]};
    r.m[c * 3] = col.x;
    r.m[c * 3 + 1] = col.y;
    r.m[c * 3 + 2] = col.z;
  }
  return r;
}

std::optional<Mat3> Mat3::inverse() const noexcept {
  const float a = m[0], b = m[3], c = m[6];
  const float d = m[1], e = m[4], f = m[7];
  const float g = m[2], h = m[5], i = m[8];
  const float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
  const float det = a * A + b * B + c * C;
  if (std::fabs(det) < 1e-6f) return std::nullopt;
  const float D = -(b * i - c * h), E = a * i - c * g, F = -(a * h - b * g);
  const float G = b * f - c * e, H = -(a * f - c * d), I = a * e - b * d;
  Mat3 r;
  const float k = 1.0f / det;
  r.m[0] = A * k, r.m[3] = D * k, r.m[6] = G * k;
  r.m[1] = B * k, r.m[4] = E * k, r.m[7] = H * k;
  r.m[2] = C * k, r.m[5] = F * k, r.m[8] = I * k;
  return r;
}

Vec3 projectPlane(const Mat4& t, Vec2 p) noexcept { return Mat3::planeOf(t) * Vec3{p.x, p.y, 1}; }

std::optional<Vec2> unprojectPlane(const Mat4& t, Vec2 s) noexcept {
  const std::optional<Mat3> inv = Mat3::planeOf(t).inverse();
  if (!inv) return std::nullopt;
  const Vec3 q = *inv * Vec3{s.x, s.y, 1};
  if (std::fabs(q.z) < 1e-9f) return std::nullopt;
  const Vec2 p{q.x / q.z, q.y / q.z};
  if (projectPlane(t, p).z <= 0) return std::nullopt;
  return p;
}

bool Mat4::is3D(float eps) const noexcept {
  // 2D affine only: tx/ty (m[12], m[13]) are not 3D.
  auto nz = [eps](float v) { return std::fabs(v) > eps; };
  return nz(m[2]) || nz(m[3]) || nz(m[6]) || nz(m[7]) || nz(m[8]) || nz(m[9]) || nz(m[11]) ||
         nz(m[14]) || nz(m[10] - 1.0f) || nz(m[15] - 1.0f);
}

}  // namespace glim
