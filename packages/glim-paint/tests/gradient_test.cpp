#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Reuse.h>
#include <glim/paint/Software.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace glim;
using namespace glim::paint;

int failures = 0;

void expect(bool ok, const char* msg) {
    if (!ok) {
        std::cerr << "FAIL: " << msg << '\n';
        ++failures;
    }
}

bool near(float a, float b, float eps = 2.f) {
    return std::fabs(a - b) <= eps;
}

glim::paint::Matter linearBW(float x0, float x1) {
    const glim::paint::GradientStop stops[2] = {
        {0.f, glim::Color{0, 0, 0, 255}},
        {1.f, glim::Color{255, 255, 255, 255}},
    };
    return glim::paint::Matter::linearGradient({x0, 0.f}, {x1, 0.f}, stops, 2);
}

}  // namespace

int main() {
    using namespace glim::paint;

    // 1. Factories sort stops and degrade color to the first stop.
    {
        const GradientStop unsorted[3] = {
            {1.f, Color{0, 0, 255, 255}},
            {0.f, Color{255, 0, 0, 255}},
            {0.5f, Color{0, 255, 0, 255}},
        };
        const Matter m = Matter::linearGradient({0, 0}, {100, 0}, unsorted, 3);
        expect(m.isGradient(), "linear factory sets Gradient kind");
        expect(m.gradientStopCount == 3, "stop count kept");
        expect(m.gradientStops[0].offset == 0.f && m.gradientStops[1].offset == 0.5f &&
                   m.gradientStops[2].offset == 1.f,
               "stops sorted by offset");
        expect(m.color.rgba == Color{255, 0, 0, 255}.rgba, "color degrades to first stop");
        const Matter clamped = Matter::radialGradient({0, 0}, 10.f, unsorted, 99);
        expect(clamped.gradientStopCount == kMaxGradientStops, "stops clamped to max");
        const Matter empty = Matter::linearGradient({0, 0}, {1, 0}, nullptr, 0);
        expect(empty.gradientStopCount == 0, "null stops give zero count");
    }

    // 2. Group scale scales radial radius.
    {
        const GradientStop stops[2] = {
            {0.f, Color{255, 0, 0, 255}},
            {1.f, Color{0, 0, 255, 255}},
        };
        Context ctx;
        ctx.setSize({200, 200});
        ctx.beginFrame();
        GroupParams g;
        g.transform = Mat4::scale(2.f, 2.f);
        ctx.pushGroup(g);
        ctx.setFill(Matter::radialGradient({25, 25}, 20.f, stops, 2));
        ctx.fill(Rect{{0, 0}, {50, 50}});
        ctx.popGroup();
        ctx.finish();
        const FramePacket packet = encode(ctx.scene());
        expect(packet.gradients.size() == 1, "scaled radial encodes");
        expect(near(packet.gradients[0].radius, 40.f), "radius scales with group");
        expect(near(packet.gradients[0].p0x, 50.f) && near(packet.gradients[0].p0y, 50.f),
               "center scales with group");
    }

    // 3. Encode: unclipped linear FillRect emits one GradientQuad.
    {
        Context ctx;
        ctx.setSize({200, 100});
        ctx.beginFrame();
        ctx.setFill(linearBW(0.f, 200.f));
        ctx.fill(Rect{{0, 0}, {200, 100}});
        ctx.finish();
        expect(ctx.scene().root.shapes.size() == 1, "one shape recorded");
        const FillRect* f = std::get_if<FillRect>(&ctx.scene().root.shapes[0]);
        expect(f && f->matter.isGradient(), "fill() preserves gradient matter");
        const FramePacket packet = encode(ctx.scene());
        expect(packet.gradients.size() == 1, "one gradient quad emitted");
        expect(packet.quads.empty(), "no solid quads for a gradient fill");
        expect(packet.isolates.empty(), "gradient fill does not isolate");
        const GradientQuad& g = packet.gradients[0];
        expect(g.kind == 0 && g.stopCount == 2, "linear kind with two stops");
        expect(g.p0x == 0.f && g.p1x == 200.f, "gradient geometry preserved");
        expect(near(g.r[0], 0.f) && near(g.r[1], 1.f), "stops premultiplied");
        expect(near(g.a[0], 1.f) && near(g.a[1], 1.f), "opaque stops");
    }

    // 4. Encode: rounded + translated gradients.
    {
        Context ctx;
        ctx.setSize({200, 100});
        ctx.beginFrame();
        ctx.translate({20, 10});
        ctx.setFill(linearBW(0.f, 200.f));
        ctx.fillRounded(Rect{{0, 0}, {100, 50}}, Radius{8.f});
        ctx.finish();
        const FramePacket packet = encode(ctx.scene());
        expect(!packet.gradients.empty(), "rounded gradient emits spans");
        for (const GradientQuad& g : packet.gradients) {
            expect(g.p0x == 20.f && g.p1x == 220.f, "translation baked into gradient geometry");
            expect(g.coverage > 0.f && g.coverage <= 1.f, "coverage in range");
        }
        const GradientStop stops[2] = {
            {0.f, Color{255, 0, 0, 255}},
            {1.f, Color{0, 0, 255, 255}},
        };
        Context rc;
        rc.setSize({100, 100});
        rc.beginFrame();
        rc.setFill(Matter::radialGradient({50, 50}, 40.f, stops, 2));
        rc.fill(Rect{{10, 10}, {80, 80}});
        rc.finish();
        const FramePacket rp = encode(rc.scene());
        expect(rp.gradients.size() == 1 && rp.gradients[0].kind == 1, "radial gradient encodes");
        expect(near(rp.gradients[0].radius, 40.f), "radial radius preserved");
    }

    // 5. CPU: linear black->white ramp.
    {
        Context ctx;
        ctx.setSize({100, 10});
        ctx.beginFrame();
        ctx.setFill(linearBW(0.f, 100.f));
        ctx.fill(Rect{{0, 0}, {100, 10}});
        ctx.finish();
        std::vector<std::uint8_t> px(100 * 10 * 4, 0);
        rasterScene(ctx.scene(), 100, 10, px.data());
        const auto at = [&](int x) -> float { return static_cast<float>(px[(5 * 100 + x) * 4]); };
        expect(at(1) < 8.f, "left edge near black");
        expect(at(98) > 247.f, "right edge near white");
        expect(near(at(50), 127.5f, 3.f), "middle near half gray");
    }

    // 6. CPU: radial red->blue disc.
    {
        const GradientStop stops[2] = {
            {0.f, Color{255, 0, 0, 255}},
            {1.f, Color{0, 0, 255, 255}},
        };
        Context ctx;
        ctx.setSize({100, 100});
        ctx.beginFrame();
        ctx.setFill(Matter::radialGradient({50, 50}, 40.f, stops, 2));
        ctx.fill(Rect{{0, 0}, {100, 100}});
        ctx.finish();
        std::vector<std::uint8_t> px(100 * 100 * 4, 0);
        rasterScene(ctx.scene(), 100, 100, px.data());
        const auto red = [&](int x, int y) -> float {
            return static_cast<float>(px[(y * 100 + x) * 4]);
        };
        const auto blue = [&](int x, int y) -> float {
            return static_cast<float>(px[(y * 100 + x) * 4 + 2]);
        };
        expect(red(50, 50) > 247.f && blue(50, 50) < 8.f, "center is red");
        expect(blue(50, 95) > 247.f && red(50, 95) < 8.f, "rim clamps to blue");
    }

    // 7. Reuse: gradient scenes hit, translate, and miss correctly.
    {
        auto record = [](Context& c, float dx, std::uint32_t c0) {
            c.setSize({200, 100});
            c.beginFrame();
            if (dx != 0.f) {
                c.translate({dx, 0.f});
            }
            const GradientStop stops[2] = {
                {0.f, Color{c0}},
                {1.f, Color{255, 255, 255, 255}},
            };
            c.setFill(Matter::linearGradient({0, 0}, {200, 0}, stops, 2));
            c.fill(Rect{{0, 0}, {200, 100}});
            c.finish();
        };
        ReuseCache cache;
        Context a;
        record(a, 0.f, 0x000000ff);
        const FramePacket first = cache.encode(a.scene(), 1.f);
        expect(first.stats.reuseMisses == 1, "first gradient encode misses");
        Context b;
        record(b, 0.f, 0x000000ff);
        const FramePacket second = cache.encode(b.scene(), 1.f);
        expect(second.stats.reuseHits == 1 && second.stats.dirtyTiles == 0, "identical hits clean");
        expect(second.gradients.size() == first.gradients.size(), "hit reuses gradient quads");
        Context c;
        record(c, 32.f, 0x000000ff);
        const FramePacket moved = cache.encode(c.scene(), 1.f);
        expect(moved.stats.reuseHits == 1, "translated gradient scene hits");
        expect(!moved.gradients.empty() && near(moved.gradients[0].x, first.gradients[0].x + 32.f),
               "translation shifts gradient quads and geometry");
        expect(near(moved.gradients[0].p0x, first.gradients[0].p0x + 32.f),
               "translation shifts gradient points");
        Context d;
        record(d, 0.f, 0x00ff00ff);
        const FramePacket changed = cache.encode(d.scene(), 1.f);
        expect(changed.stats.reuseMisses == 1, "recolor misses");
    }

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "gradient_test ok\n";
    return EXIT_SUCCESS;
}
