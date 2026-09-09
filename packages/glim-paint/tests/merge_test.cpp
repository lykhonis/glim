#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>

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

}  // namespace

int main() {
    glim::Mat4 id = glim::Mat4::identity();
    expect(!id.is3D(), "identity is not 3D");
    expect(!glim::Mat4::translate(100, 50).is3D(), "tx/ty is not 3D");
    glim::Mat4 persp = id;
    persp.m[11] = -1.0f / 800.0f;
    expect(persp.is3D(), "perspective w-row is 3D");

    glim::paint::Context ctx;
    ctx.setSize({720, 480});
    ctx.beginFrame();
    ctx.setFillColor(0x334c4cff);
    ctx.fill(glim::Rect::fromSize({720, 480}));
    ctx.translate({100, 50});
    ctx.setFillColor(0xac6363ff);
    ctx.fill(glim::Rect::fromSize({400, 300}));
    ctx.finish();

    glim::paint::Stats stats{};
    glim::paint::Group merged = glim::paint::merge(std::move(ctx.scene().root), &stats);
    expect(merged.children.empty(), "opaque 2D children merge into root");
    expect(merged.shapes.size() == 2, "two fills after merge");
    expect(stats.mergedGroupCount == 0, "no extra groups in flat hello");

    glim::paint::Context grouped;
    grouped.setSize({720, 480});
    grouped.beginFrame();
    grouped.setFillColor(0xff0000ff);
    grouped.fill(glim::Rect::fromSize({10, 10}));
    glim::paint::GroupParams p;
    p.opacity = 1.0f;
    p.transform = glim::Mat4::translate(5, 6);
    grouped.pushGroup(p);
    grouped.setFillColor(0x00ff00ff);
    grouped.fill(glim::Rect::fromSize({4, 4}));
    grouped.popGroup();
    grouped.finish();
    stats = {};
    merged = glim::paint::merge(std::move(grouped.scene().root), &stats);
    expect(merged.children.empty(), "opaque group merged");
    expect(merged.shapes.size() == 2, "parent fill + merged child fill");
    expect(stats.mergedGroupCount == 1, "one group collapsed");
    auto* childFill = std::get_if<glim::paint::FillRect>(&merged.shapes[1]);
    expect(childFill && glim::nearlyEqual(childFill->rect.origin.x, 5.f), "merged child origin x");
    expect(childFill && glim::nearlyEqual(childFill->rect.origin.y, 6.f), "merged child origin y");

    glim::paint::Context glass;
    glass.setSize({100, 100});
    glass.beginFrame();
    glim::paint::GroupParams g;
    g.opacity = 0.5f;
    g.bounds = glim::Rect::fromSize({100, 20});
    glass.pushGroup(g);
    glass.setFillColor(0xffffffff);
    glass.fill(glim::Rect::fromSize({100, 20}));
    glass.popGroup();
    glass.finish();
    stats = {};
    merged = glim::paint::merge(std::move(glass.scene().root), &stats);
    expect(merged.children.size() == 1, "translucent group stays isolated");
    expect(glim::paint::needsIsolate(*merged.children[0]), "opacity isolates");

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "merge_test ok\n";
    return EXIT_SUCCESS;
}
