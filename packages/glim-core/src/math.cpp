#include <glim/math.h>

namespace glim {

Mat4 Mat4::identity() noexcept {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4::orthoYDown(float left, float top, float right, float bottom, float zNear, float zFar) noexcept {
    // Maps paint Y-down (y=top at window top) into Y-up NDC (Metal / GL):
    //   x: left → -1, right → +1
    //   y: top  → +1, bottom → -1
    // Equivalent to glm::ortho(left, right, bottom, top, zNear, zFar).
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

bool Mat4::is3D(float eps) const noexcept {
    // 2D affine in XY: Z basis is (0,0,1), w-row is (0,0,0,1). tx/ty (m[12], m[13]) are not 3D.
    auto nz = [eps](float v) { return std::fabs(v) > eps; };
    return nz(m[2]) || nz(m[3]) || nz(m[6]) || nz(m[7]) || nz(m[8]) || nz(m[9]) || nz(m[11]) ||
           nz(m[14]) || nz(m[10] - 1.0f) || nz(m[15] - 1.0f);
}

}  // namespace glim
