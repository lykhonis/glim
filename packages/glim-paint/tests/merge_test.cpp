#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>

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
    expect(childFill && childFill->matter.kind == glim::paint::MatterKind::Solid, "merged fill is solid matter");
    expect(childFill && childFill->matter.color.rgba == 0x00ff00ff, "merged fill keeps color");

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

    glim::paint::Context blitCtx;
    blitCtx.setSize({100, 100});
    blitCtx.beginFrame();
    blitCtx.setFillColor(0x0000ffff);
    blitCtx.fill(glim::Rect::fromSize({100, 100}));
    const std::uint8_t px[4] = {255, 0, 0, 255};
    const std::uint32_t imageId = blitCtx.addImage(1, 1, px);
    expect(imageId != 0, "addImage returns id");
    blitCtx.blit(glim::Rect{{10, 10}, {20, 20}}, imageId);
    blitCtx.finish();
    stats = {};
    merged = glim::paint::merge(std::move(blitCtx.scene().root), &stats);
    expect(merged.shapes.size() == 2, "fill + blit after merge");
    expect(std::get_if<glim::paint::Blit>(&merged.shapes[1]) != nullptr, "second shape is Blit");
    expect(blitCtx.addImage(5000, 1, px) == 0, "reject oversize side");

    glim::paint::Context clipCtx;
    clipCtx.setSize({100, 100});
    clipCtx.beginFrame();
    clipCtx.setFillColor(0xff0000ff);
    clipCtx.fill(glim::Rect::fromSize({100, 40}));
    glim::paint::GroupParams clipParams;
    clipParams.clip = glim::Rect{{10, 0}, {80, 40}};
    clipCtx.pushGroup(clipParams);
    clipCtx.setFillColor(0x00ff00ff);
    clipCtx.fill(glim::Rect{{0, 0}, {200, 40}});
    clipCtx.popGroup();
    clipCtx.finish();
    stats = {};
    merged = glim::paint::merge(std::move(clipCtx.scene().root), &stats);
    expect(merged.children.size() == 1, "clip-only group is not merged");
    expect(!glim::paint::needsIsolate(*merged.children[0]), "clip does not isolate");
    expect(glim::paint::hasClip(merged.children[0]->params), "clip stays on GroupParams");
    expect(merged.shapes.size() == 1, "parent background is not folded into clip");
    auto* bg = std::get_if<glim::paint::FillRect>(&merged.shapes[0]);
    expect(bg && bg->rect.size.x >= 99.f, "scroll-clip keeps unclipped background");

    glim::paint::Context roundCtx;
    roundCtx.setSize({80, 80});
    roundCtx.beginFrame();
    roundCtx.setFillColor(0xffffffff);
    roundCtx.fillRounded(glim::Rect::fromSize({40, 40}), glim::Radius{8.f});
    roundCtx.strokeRect(glim::Rect{{4, 4}, {32, 32}}, glim::Radius{6.f}, 2.f);
    roundCtx.finish();
    merged = glim::paint::merge(std::move(roundCtx.scene().root), nullptr);
    expect(std::get_if<glim::paint::FillRounded>(&merged.shapes[0]) != nullptr, "fillRounded records");
    expect(std::get_if<glim::paint::Stroke>(&merged.shapes[1]) != nullptr, "stroke records");

    glim::paint::Context slotCtx;
    slotCtx.setSize({80, 80});
    slotCtx.beginFrame();
    slotCtx.setFillColor(0xffffffff);
    slotCtx.fill(glim::Rect::fromSize({80, 80}));
    glim::paint::GroupParams slotGroup;
    slotCtx.pushGroup(slotGroup);
    slotCtx.slot(glim::Rect{{8, 8}, {24, 16}}, 1);
    slotCtx.popGroup();
    slotCtx.finish();
    stats = {};
    merged = glim::paint::merge(std::move(slotCtx.scene().root), &stats);
    expect(merged.children.size() == 1, "SlotHole child is not merged");
    expect(merged.children.size() == 1 && glim::paint::hasSlotHole(*merged.children[0]),
           "merged tree keeps SlotHole");

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "merge_test ok\n";
    return EXIT_SUCCESS;
}
