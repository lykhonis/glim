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
    glim::paint::Context ctx;
    ctx.setSize({100, 100});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(glim::Rect::fromSize({100, 100}));
    ctx.setFillColor(0x00ff00ff);
    ctx.fill(glim::Rect{{10, 10}, {20, 20}});
    ctx.finish();

    glim::paint::Hit hit{};
    expect(glim::paint::hitTest(ctx.scene(), {15, 15}, &hit), "front fill hits");
    expect(hit.semantic == 0, "no semantic by default");
    expect(!hit.slot, "fill is not a slot");
    expect(hit.opaque, "hit is opaque");
    expect(glim::paint::hitTest(ctx.scene(), {5, 5}, &hit), "background hits");
    expect(!glim::paint::hitTest(ctx.scene(), {200, 200}, &hit), "outside misses");
    expect(!glim::paint::hitTest(ctx.scene(), {15, 15}, nullptr), "null out misses");

    glim::paint::Context sem;
    sem.setSize({100, 100});
    sem.beginFrame();
    sem.setFillColor(0xff0000ff);
    sem.fill(glim::Rect::fromSize({100, 100}));
    glim::paint::GroupParams card;
    card.semantic = 42;
    sem.pushGroup(card);
    sem.setFillColor(0x00ff00ff);
    sem.fill(glim::Rect{{10, 10}, {20, 20}});
    sem.popGroup();
    sem.finish();

    expect(glim::paint::hitTest(sem.scene(), {15, 15}, &hit), "semantic card hits");
    expect(hit.semantic == 42, "group semantic returned");
    expect(glim::nearlyEqual(hit.bounds.origin.x, 10.f), "hit bounds x");
    expect(glim::paint::hitTest(sem.scene(), {5, 5}, &hit), "background hits");
    expect(hit.semantic == 0, "background has no semantic");

    glim::paint::Group merged = glim::paint::merge(std::move(sem.scene().root), nullptr);
    expect(merged.children.size() == 1, "semantic group does not merge");
    expect(!glim::paint::canMerge(*merged.children[0]), "canMerge refuses semantic");

    glim::paint::Context nested;
    nested.setSize({100, 100});
    nested.beginFrame();
    glim::paint::GroupParams outer;
    outer.semantic = 7;
    nested.pushGroup(outer);
    nested.setFillColor(0xff0000ff);
    nested.fill(glim::Rect::fromSize({100, 100}));
    glim::paint::GroupParams inner;
    inner.semantic = 9;
    nested.pushGroup(inner);
    nested.setFillColor(0x00ff00ff);
    nested.fill(glim::Rect{{10, 10}, {20, 20}});
    nested.popGroup();
    nested.popGroup();
    nested.finish();

    expect(glim::paint::hitTest(nested.scene(), {15, 15}, &hit), "nested hits");
    expect(hit.semantic == 9, "innermost semantic wins");
    expect(glim::paint::hitTest(nested.scene(), {50, 50}, &hit), "outer region hits");
    expect(hit.semantic == 7, "outer semantic returned");

    glim::paint::Context moved;
    moved.setSize({100, 100});
    moved.beginFrame();
    glim::paint::GroupParams shift;
    shift.transform = glim::Mat4::translate(30, 40);
    shift.semantic = 5;
    moved.pushGroup(shift);
    moved.setFillColor(0xffffffff);
    moved.fill(glim::Rect::fromSize({10, 10}));
    moved.popGroup();
    moved.finish();

    expect(!glim::paint::hitTest(moved.scene(), {5, 5}, &hit), "unshifted misses");
    expect(glim::paint::hitTest(moved.scene(), {35, 45}, &hit), "shifted hits");
    expect(hit.semantic == 5, "shifted semantic returned");

    glim::paint::Context clipped;
    clipped.setSize({100, 100});
    clipped.beginFrame();
    glim::paint::GroupParams clipP;
    clipP.clip = glim::Rect{{10, 10}, {20, 20}};
    clipP.semantic = 11;
    clipped.pushGroup(clipP);
    clipped.setFillColor(0xffffffff);
    clipped.fill(glim::Rect::fromSize({100, 100}));
    clipped.popGroup();
    clipped.finish();

    expect(glim::paint::hitTest(clipped.scene(), {15, 15}, &hit), "inside clip hits");
    expect(hit.semantic == 11, "clipped semantic returned");
    expect(!glim::paint::hitTest(clipped.scene(), {50, 50}, &hit), "outside clip misses");

    glim::paint::Context slots;
    slots.setSize({100, 100});
    slots.beginFrame();
    slots.setFillColor(0xff0000ff);
    slots.fill(glim::Rect::fromSize({100, 100}));
    slots.slot(glim::Rect{{20, 20}, {30, 30}}, 3);
    slots.finish();

    expect(glim::paint::hitTest(slots.scene(), {25, 25}, &hit), "slot hits");
    expect(hit.slot, "slot flag set");

    glim::paint::Context blitCtx;
    blitCtx.setSize({100, 100});
    blitCtx.beginFrame();
    const std::uint8_t px[4] = {255, 0, 0, 255};
    const std::uint32_t imageId = blitCtx.addImage(1, 1, px);
    blitCtx.blit(glim::Rect{{40, 40}, {10, 10}}, imageId);
    blitCtx.finish();

    expect(glim::paint::hitTest(blitCtx.scene(), {45, 45}, &hit), "blit hits as quad");
    expect(!hit.slot, "blit is not a slot");
    expect(!glim::paint::hitTest(blitCtx.scene(), {5, 5}, &hit), "outside blit misses");

    glim::paint::Context setter;
    setter.setSize({100, 100});
    setter.beginFrame();
    setter.setSemantic(77);
    setter.setFillColor(0xffffffff);
    setter.fill(glim::Rect::fromSize({10, 10}));
    setter.finish();

    expect(glim::paint::hitTest(setter.scene(), {5, 5}, &hit), "setSemantic hits");
    expect(hit.semantic == 77, "setSemantic id returned");

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "hit_test ok\n";
    return EXIT_SUCCESS;
}
