#include <glim/paint/Context.h>
#include <glim/paint/Overlay.h>
#include <glim/paint/Scene.h>

#include <cstdlib>
#include <iostream>
#include <variant>

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    glim::paint::Overlay overlay;
    expect(!overlay.enabled(), "disabled by default");
    overlay.setEnabled(true);
    expect(overlay.enabled(), "setEnabled true");
    overlay.setEnabled(false);
    expect(!overlay.enabled(), "setEnabled false");

    overlay.setEnabled(true);
    expect(!overlay.expanded(), "compact by default");
    overlay.toggleExpanded();
    expect(overlay.expanded(), "toggle expands");
    overlay.toggleExpanded();
    expect(!overlay.expanded(), "toggle collapses");
    for (int i = 0; i < 60; ++i) {
        overlay.tick(1.f / 60.f);
    }
    overlay.tick(0.f);
    overlay.tick(2.f);

    glim::paint::Context ctx;
    ctx.setSize({200, 120});
    ctx.beginFrame();
    overlay.setEnabled(false);
    overlay.record(ctx, glim::Rect::fromSize({200.f, 120.f}));
    expect(ctx.scene().root.shapes.empty(), "disabled record is a no-op");
    expect(ctx.scene().root.children.empty(), "disabled record adds no groups");

    overlay.setEnabled(true);
    glim::paint::Stats stats{};
    stats.encodeMs = 1.25f;
    stats.draws = 3;
    stats.instances = 12;
    stats.isolateCount = 0;
    overlay.setStats(stats);
    overlay.record(ctx, glim::Rect::fromSize({200.f, 120.f}));
    ctx.finish();

    expect(!ctx.scene().root.shapes.empty(), "enabled record paints");
    expect(ctx.scene().root.children.empty(), "overlay does not push groups");
    expect(!glim::paint::needsIsolate(ctx.scene().root), "overlay does not isolate");

    bool hasGlyphs = false;
    bool hasFill = false;
    for (const glim::paint::Shape& s : ctx.scene().root.shapes) {
        if (std::holds_alternative<glim::paint::GlyphRun>(s)) {
            hasGlyphs = true;
        }
        if (std::holds_alternative<glim::paint::FillRect>(s) ||
            std::holds_alternative<glim::paint::FillRounded>(s)) {
            hasFill = true;
        }
    }
    expect(hasFill, "panel fill");
    expect(hasGlyphs, "latin glyph runs");

    if (failures != 0) {
        std::cerr << failures << " overlay tests failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "overlay_test ok\n";
    return EXIT_SUCCESS;
}
