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

    glim::paint::Context fade;
    fade.setSize({100, 100});
    fade.beginFrame();
    glim::paint::GroupParams g;
    g.opacity = 0.5f;
    g.bounds = glim::Rect::fromSize({100, 20});
    fade.pushGroup(g);
    fade.setFillColor(0xffffffff);
    fade.fill(glim::Rect::fromSize({100, 20}));
    fade.popGroup();
    fade.finish();
    stats = {};
    merged = glim::paint::merge(std::move(fade.scene().root), &stats);
    expect(merged.children.size() == 1, "translucent group stays isolated");
    expect(glim::paint::needsIsolate(*merged.children[0]), "opacity isolates");

    glim::paint::Context frost;
    frost.setSize({100, 100});
    frost.beginFrame();
    frost.setFillColor(0xff0000ff);
    frost.fill(glim::Rect::fromSize({100, 100}));
    glim::paint::GroupParams blur;
    blur.backdropBlur = 8.f;
    blur.bounds = glim::Rect{{10, 10}, {40, 20}};
    frost.pushGroup(blur);
    frost.setFillColor(0xffffff88);
    frost.fill(glim::Rect{{10, 10}, {40, 20}});
    frost.popGroup();
    frost.finish();
    stats = {};
    merged = glim::paint::merge(std::move(frost.scene().root), &stats);
    expect(merged.children.size() == 1, "backdrop group stays isolated");
    expect(merged.children.size() == 1 && glim::paint::hasBackdrop(*merged.children[0]),
           "backdropBlur marks backdrop");
    expect(merged.children.size() == 1 && glim::paint::needsIsolate(*merged.children[0]),
           "backdrop isolates");
    expect(!glim::paint::canMerge(*merged.children[0]), "canMerge refuses backdrop");
    expect(glim::nearlyEqual(glim::paint::snapBackdropSigma(7.f), 8.f), "sigma snaps to 8");
    expect(glim::paint::isolatePixelSize(100.f, 2.f) == 200, "isolate size uses pixelRatio");

    glim::paint::Context lens;
    lens.setSize({100, 100});
    lens.beginFrame();
    lens.setFillColor(0xff0000ff);
    lens.fill(glim::Rect::fromSize({100, 100}));
    glim::paint::GroupParams glass;
    glass.glass = glim::paint::Glass{};
    glass.bounds = glim::Rect{{10, 10}, {40, 20}};
    lens.pushGroup(glass);
    lens.popGroup();
    lens.finish();
    stats = {};
    merged = glim::paint::merge(std::move(lens.scene().root), &stats);
    expect(merged.children.size() == 1, "glass group stays isolated");
    expect(merged.children.size() == 1 && glim::paint::hasGlass(*merged.children[0]), "Glass marks glass");
    expect(merged.children.size() == 1 && !glim::paint::hasBackdrop(*merged.children[0]),
           "glass is independent of backdrop");
    expect(merged.children.size() == 1 && glim::paint::needsIsolate(*merged.children[0]), "glass isolates");
    expect(!glim::paint::canMerge(*merged.children[0]), "canMerge refuses glass");

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

    expect(glim::paint::blendCompatible(glim::paint::Blend::SrcOver, glim::paint::Blend::SrcOver),
           "SrcOver merges into SrcOver");
    expect(glim::paint::blendCompatible(glim::paint::Blend::Plus, glim::paint::Blend::Plus),
           "Plus merges into Plus");
    expect(!glim::paint::blendCompatible(glim::paint::Blend::SrcOver, glim::paint::Blend::Plus),
           "mixed blends never share a pass");
    expect(!glim::paint::blendCompatible(glim::paint::Blend::Plus, glim::paint::Blend::SrcOver),
           "mixed blends never share a pass (reversed)");

    glim::paint::Context plusCtx;
    plusCtx.setSize({80, 80});
    plusCtx.beginFrame();
    glim::paint::GroupParams plusParent;
    plusParent.blend = glim::paint::Blend::Plus;
    plusCtx.pushGroup(plusParent);
    plusCtx.setFillColor(0xffffffff);
    plusCtx.fill(glim::Rect::fromSize({10, 10}));
    glim::paint::GroupParams plusChild;
    plusChild.blend = glim::paint::Blend::Plus;
    plusCtx.pushGroup(plusChild);
    plusCtx.setFillColor(0xffffffff);
    plusCtx.fill(glim::Rect::fromSize({4, 4}));
    plusCtx.popGroup();
    plusCtx.popGroup();
    plusCtx.finish();
    stats = {};
    merged = glim::paint::merge(std::move(plusCtx.scene().root), &stats);
    expect(merged.children.size() == 1, "Plus parent stays isolated from SrcOver root");
    expect(merged.children.size() == 1 && merged.children[0]->children.empty(),
           "both-Plus child folds into Plus parent");

    glim::paint::Context mixedCtx;
    mixedCtx.setSize({80, 80});
    mixedCtx.beginFrame();
    mixedCtx.setFillColor(0xffffffff);
    mixedCtx.fill(glim::Rect::fromSize({80, 80}));
    glim::paint::GroupParams plusKid;
    plusKid.blend = glim::paint::Blend::Plus;
    mixedCtx.pushGroup(plusKid);
    mixedCtx.setFillColor(0xffffffff);
    mixedCtx.fill(glim::Rect::fromSize({10, 10}));
    mixedCtx.popGroup();
    mixedCtx.finish();
    stats = {};
    merged = glim::paint::merge(std::move(mixedCtx.scene().root), &stats);
    expect(merged.children.size() == 1, "Plus child does not merge into SrcOver parent");

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "merge_test ok\n";
    return EXIT_SUCCESS;
}
