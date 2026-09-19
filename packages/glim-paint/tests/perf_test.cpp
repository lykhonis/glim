// Phase 0 baseline harness (measure-only, no behavior change).
//
// Records a hello-class scene and a typical-GUI scene (<=200 rects/glyphs,
// <=4 isolates), times merge+encode, and prints Stats. Structural budgets are
// asserted; timings are reported for the log (no hard timing assert — CI
// variance). Budgets: hello 1 instanced group, typical GUI <4ms target,
// isolates <= 4, golden pixels unchanged.

#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>

#include <chrono>
#include <cstdint>
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

void recordHello(glim::paint::Context& ctx) {
    ctx.setSize({720, 480});
    ctx.beginFrame();
    ctx.setFillColor(0x334c4cff);
    ctx.fill(glim::Rect::fromSize({720, 480}));
    ctx.translate({100, 50});
    ctx.setFillColor(0xac6363ff);
    ctx.fill(glim::Rect::fromSize({400, 300}));
    ctx.finish();
}

// Typical GUI: background + card grid (fills + rounded) + glyph runs +
// scrolling clip + opacity isolates + one backdrop frost + one glass bar.
void recordTypical(glim::paint::Context& ctx) {
    ctx.setSize({720, 480});
    ctx.beginFrame();
    ctx.setFillColor(0x1a1d21ff);
    ctx.fill(glim::Rect::fromSize({720, 480}));
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 12; ++j) {
            const float x = 16.f + static_cast<float>(j) * 58.f;
            const float y = 16.f + static_cast<float>(i) * 40.f;
            ctx.setFillColor(((i + j) % 2 == 0) ? 0x2a2f36ff : 0x23282eff);
            if ((i + j) % 3 == 0) {
                ctx.fillRounded(glim::Rect{{x, y}, {50, 32}}, glim::Radius{6.f});
            } else {
                ctx.fill(glim::Rect{{x, y}, {50, 32}});
            }
        }
    }
    ctx.setFillColor(0xffffffff);
    for (int i = 0; i < 12; ++i) {
        ctx.text({20.f, 420.f + static_cast<float>(i % 3) * 18.f}, "Hello Glim", 14.f);
    }
    // Scrolling clip (clip-only, no isolate).
    glim::paint::GroupParams clip;
    clip.clip = glim::Rect{{16, 300}, {688, 100}};
    ctx.pushGroup(clip);
    ctx.setFillColor(0x00ff00ff);
    ctx.fill(glim::Rect{{0, 300}, {900, 100}});
    ctx.popGroup();
    // Opacity isolate.
    glim::paint::GroupParams fade;
    fade.opacity = 0.5f;
    fade.bounds = glim::Rect{{16, 410}, {200, 40}};
    ctx.pushGroup(fade);
    ctx.setFillColor(0xffffffff);
    ctx.fill(glim::Rect{{16, 410}, {200, 40}});
    ctx.popGroup();
    // Backdrop frost isolate.
    glim::paint::GroupParams frost;
    frost.backdropBlur = 8.f;
    frost.bounds = glim::Rect{{480, 410}, {224, 54}};
    ctx.pushGroup(frost);
    ctx.setFillColor(0xffffff66);
    ctx.fillRounded(glim::Rect{{480, 410}, {224, 54}}, glim::Radius{12.f});
    ctx.popGroup();
    // Liquid Glass chrome isolate.
    glim::paint::GroupParams chrome;
    chrome.glass = glim::paint::Glass{};
    chrome.bounds = glim::Rect{{16, 16}, {688, 44}};
    ctx.pushGroup(chrome);
    ctx.fillRounded(glim::Rect{{16, 16}, {688, 44}}, glim::Radius{22.f});
    ctx.popGroup();
    ctx.finish();
}

struct Timing {
    double avgMs = 0;
    glim::paint::FramePacket last;
};

Timing timeEncode(glim::paint::Context& ctx, int iters) {
    using Clock = std::chrono::steady_clock;
    Timing out;
    double total = 0;
    for (int i = 0; i < iters; ++i) {
        const auto t0 = Clock::now();
        out.last = glim::paint::encode(ctx.scene(), 1.f);
        total += std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    }
    out.avgMs = total / static_cast<double>(iters);
    return out;
}

}  // namespace

int main() {
    constexpr int kIters = 20;

    glim::paint::Context hello;
    recordHello(hello);
    const Timing h = timeEncode(hello, kIters);
    std::cout << "hello: quads=" << h.last.quads.size() << " blits=" << h.last.blits.size()
              << " isolates=" << h.last.isolates.size()
              << " instances=" << h.last.stats.instances
              << " mergedGroups=" << h.last.stats.mergedGroupCount
              << " tiles=" << h.last.stats.tileCount << " avgMs=" << h.avgMs << '\n';
    expect(h.last.quads.size() == 2, "hello merges to 2 quads (one instanced group)");
    expect(h.last.isolates.empty(), "hello has no isolates");
    expect(h.last.stats.tileCount > 0, "hello reports tile count");

    glim::paint::Context gui;
    recordTypical(gui);
    const Timing g = timeEncode(gui, kIters);
    const unsigned guiInstances =
        static_cast<unsigned>(g.last.quads.size() + g.last.blits.size());
    std::cout << "typical: quads=" << g.last.quads.size() << " blits=" << g.last.blits.size()
              << " isolates=" << g.last.isolates.size() << " instances=" << guiInstances
              << " mergedGroups=" << g.last.stats.mergedGroupCount
              << " tiles=" << g.last.stats.tileCount
              << " backdrop=" << g.last.stats.backdropCount
              << " glassPasses=" << g.last.stats.glassPassCount << " avgMs=" << g.avgMs << '\n';
    expect(g.last.isolates.size() <= 4, "typical GUI uses <=4 isolates");
    expect(guiInstances > 100, "typical GUI encodes >100 instances");
    expect(g.last.stats.tileCount > 0, "typical GUI reports tile count");
    expect(g.last.stats.backdropCount == 1, "typical GUI has one backdrop pyramid");
    expect(g.last.stats.glassPassCount == 1, "typical GUI has one glass pass");

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "perf_test ok\n";
    return EXIT_SUCCESS;
}
