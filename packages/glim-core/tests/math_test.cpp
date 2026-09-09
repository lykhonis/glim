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
    // Pin Y-down setOrthogonal(0,0,w,h) against glm::ortho(0, w, h, 0, -1, 1)
    // for the hello canvas 720×480.
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

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "math_test ok\n";
    return EXIT_SUCCESS;
}
