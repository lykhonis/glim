#include <glim/math.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

void expectNear(float got, float want, const char* what, float eps = 1e-5f) {
  if (std::fabs(got - want) > eps) {
    std::cerr << "FAIL: " << what << " got " << got << " want " << want << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  // Pin Y-down ortho against glm::ortho for the 720x480 golden.
  const float w = 720.0f;
  const float h = 480.0f;
  const glim::Mat4 p = glim::Mat4::orthoYDown(0.0f, 0.0f, w, h, -1.0f, 1.0f);

  expectNear(p.m[0], 2.0f / w, "sx");
  expectNear(p.m[1], 0.0f, "m10");
  expectNear(p.m[2], 0.0f, "m20");
  expectNear(p.m[3], 0.0f, "m30");
  expectNear(p.m[4], 0.0f, "m01");
  expectNear(p.m[5], 2.0f / (0.0f - h), "sy = -2/h");
  expectNear(p.m[6], 0.0f, "m21");
  expectNear(p.m[7], 0.0f, "m31");
  expectNear(p.m[8], 0.0f, "m02");
  expectNear(p.m[9], 0.0f, "m12");
  expectNear(p.m[10], -1.0f, "sz");
  expectNear(p.m[11], 0.0f, "m32");
  expectNear(p.m[12], -1.0f, "tx");
  expectNear(p.m[13], 1.0f, "ty");
  expectNear(p.m[14], 0.0f, "tz");
  expectNear(p.m[15], 1.0f, "tw");

  const glim::Vec4 topLeft = p * glim::Vec4{0, 0, 0, 1};
  const glim::Vec4 bottomRight = p * glim::Vec4{w, h, 0, 1};
  expectNear(topLeft.x, -1.0f, "top-left NDC x");
  expectNear(topLeft.y, 1.0f, "top-left NDC y (window top → NDC +Y)");
  expectNear(bottomRight.x, 1.0f, "bottom-right NDC x");
  expectNear(bottomRight.y, -1.0f, "bottom-right NDC y");

  const glim::Mat4 t = glim::Mat4::translate(100.0f, 50.0f);
  const glim::Vec4 p2 = (p * t) * glim::Vec4{0, 0, 0, 1};
  const glim::Vec4 expected = p * glim::Vec4{100.0f, 50.0f, 0, 1};
  expectNear(p2.x, expected.x, "translate compose x");
  expectNear(p2.y, expected.y, "translate compose y");

  const glim::Color c{0xac, 0x63, 0x63, 0xff};
  expect(c.r() == 0xac && c.g() == 0x63 && c.b() == 0x63 && c.a() == 0xff, "color channels");
  const glim::Vec4 pm = c.premul();
  expectNear(pm.w, 1.0f, "opaque alpha");
  expectNear(pm.x, 0xac / 255.0f, "opaque premul r");

  expect(!glim::Mat4::identity().is3D(), "identity not 3D");
  expect(!glim::Mat4::translate(3, 4).is3D(), "translation not 3D");

  // A tilted plane: CSS-style perspective about the rect's center.
  {
    const float pi = 3.14159265f;
    const glim::Mat4 tilt = glim::Mat4::translate(100, 50) * glim::Mat4::perspective(400) *
                            glim::Mat4::rotateY(pi / 4) * glim::Mat4::translate(-100, -50);
    expect(tilt.is3D(), "perspective tilt is 3D");
    const glim::Vec3 left = glim::projectPlane(tilt, {0, 0});
    const glim::Vec3 right = glim::projectPlane(tilt, {200, 0});
    expect(left.z > 0 && right.z > 0, "both edges in front of the eye");
    // rotateY(+) swings the right edge away: it shrinks toward the center.
    const float leftY = left.y / left.z, rightY = right.y / right.z;
    expect(rightY > leftY, "far edge's top is lower (smaller)");
    expect(left.x / left.z < 30.0f, "near edge stays left");
    const glim::Vec3 mid = glim::projectPlane(tilt, {100, 50});
    expectNear(mid.x / mid.z, 100.0f, "pivot stays put x", 1e-3f);
    expectNear(mid.y / mid.z, 50.0f, "pivot stays put y", 1e-3f);
    for (const glim::Vec2 p : {glim::Vec2{10, 10}, glim::Vec2{190, 80}, glim::Vec2{55, 33}}) {
      const glim::Vec3 s = glim::projectPlane(tilt, p);
      const auto back = glim::unprojectPlane(tilt, {s.x / s.z, s.y / s.z});
      expect(back.has_value(), "projected points unproject");
      expectNear(back->x, p.x, "unproject x", 1e-3f);
      expectNear(back->y, p.y, "unproject y", 1e-3f);
    }
    const glim::Mat4 edgeOn = glim::Mat4::rotateY(pi / 2);
    expect(!glim::unprojectPlane(edgeOn, {0, 0}).has_value(), "edge-on plane has no inverse");
    expect(glim::Mat4::rotateX(0.3f).is3D(), "rotateX is 3D");
  }

  // Inverse undoes perspective, flips and skews; a flat matrix has none.
  {
    const glim::Mat4 m = glim::Mat4::translate(30, 40) * glim::Mat4::perspective(500) *
                         glim::Mat4::rotateY(0.4f) * glim::Mat4::skew(0.2f, 0) *
                         glim::Mat4::scale(1, -1);
    const auto inv = m.inverse();
    expect(inv.has_value(), "an invertible matrix inverts");
    const glim::Mat4 one = m * *inv;
    for (int i = 0; i < 16; ++i) {
      expectNear(one.m[i], glim::Mat4::identity().m[i], "m * inverse is identity", 1e-4f);
    }
    expect(!glim::Mat4::scale(0, 1).inverse().has_value(), "a singular matrix has no inverse");
  }

  // A fade over a rect: alpha at its points, held beyond.
  {
    const glim::AlphaRamp r = glim::rampOver({{0, 100}, {0, 50}, 0.4f, 0.f}, {{0, 0}, {10, 100}});
    expectNear(r.at({0.5f, 1.f}), 0.4f, "fromAlpha at from", 1e-5f);
    expectNear(r.at({0.5f, 0.75f}), 0.2f, "half way", 1e-5f);
    expectNear(r.at({0.5f, 0.f}), 0.f, "held beyond to", 1e-5f);
    expect(!r.opaque() && glim::AlphaRamp{}.opaque(), "only a full ramp is opaque");
    const glim::AlphaRamp flat = glim::rampOver({{5, 5}, {5, 5}, 1.f, 0.25f}, {{0, 0}, {10, 10}});
    expectNear(flat.at({0.f, 0.f}), 0.25f, "a zero-length fade holds its end", 1e-5f);
    expect(glim::isIdentity(glim::Mat4::identity()), "identity is identity");
    expect(!glim::isIdentity(glim::Mat4::skew(0.1f, 0)), "a skew is not");
  }

  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return EXIT_FAILURE;
  }
  std::cout << "math_test ok\n";
  return EXIT_SUCCESS;
}
