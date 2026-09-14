#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Renderer.h>
#include <glim/paint/Scene.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <variant>
#include <vector>

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
    ctx.setSize({200, 100});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(glim::Rect::fromSize({200, 100}));
    ctx.translate({10, 20});
    ctx.slot(glim::Rect{{0, 0}, {40, 30}}, 7);
    ctx.finish();

    expect(ctx.scene().root.shapes.size() == 2, "fill + SlotHole on root");
    const auto* hole = std::get_if<glim::paint::SlotHole>(&ctx.scene().root.shapes[1]);
    expect(hole && hole->id == 7, "recorded SlotHole id");
    expect(hole && glim::nearlyEqual(hole->rect.origin.x, 10.f), "slot origin x");
    expect(hole && glim::nearlyEqual(hole->rect.origin.y, 20.f), "slot origin y");
    expect(hole && glim::nearlyEqual(hole->rect.size.x, 40.f), "slot width");

    std::vector<glim::paint::SlotPlacement> placed;
    glim::paint::collectSlots(ctx.scene(), &placed);
    expect(placed.size() == 1, "one slot placement");
    expect(placed.size() == 1 && placed[0].id == 7, "collectSlots id");
    expect(placed.size() == 1 && glim::nearlyEqual(placed[0].windowLogical.origin.x, 10.f),
           "collectSlots x");
    expect(placed.size() == 1 && glim::nearlyEqual(placed[0].windowLogical.origin.y, 20.f),
           "collectSlots y");

    glim::paint::Context grouped;
    grouped.setSize({100, 100});
    grouped.beginFrame();
    grouped.setFillColor(0x0000ffff);
    grouped.fill(glim::Rect::fromSize({100, 100}));
    glim::paint::GroupParams p;
    p.transform = glim::Mat4::translate(8, 9);
    grouped.pushGroup(p);
    grouped.slot(glim::Rect{{1, 2}, {16, 12}}, 3);
    grouped.popGroup();
    grouped.finish();

    glim::paint::Stats stats{};
    glim::paint::Group rootCopy = grouped.scene().root;
    glim::paint::Group merged = glim::paint::merge(std::move(rootCopy), &stats);
    expect(merged.children.size() == 1, "SlotHole group does not merge");
    expect(merged.children.size() == 1 && glim::paint::hasSlotHole(*merged.children[0]),
           "kept child has SlotHole");
    expect(merged.children.size() == 1 && !glim::paint::needsIsolate(*merged.children[0]),
           "SlotHole is not isolate");
    expect(merged.children.size() == 1 && !glim::paint::canMerge(*merged.children[0]),
           "canMerge refuses SlotHole");

    placed.clear();
    glim::paint::collectSlots(grouped.scene(), &placed);
    expect(placed.size() == 1 && placed[0].id == 3, "grouped slot id");
    expect(placed.size() == 1 && glim::nearlyEqual(placed[0].windowLogical.origin.x, 9.f),
           "grouped slot x = 8+1");
    expect(placed.size() == 1 && glim::nearlyEqual(placed[0].windowLogical.origin.y, 11.f),
           "grouped slot y = 9+2");

    glim::paint::Context clipped;
    clipped.setSize({100, 100});
    clipped.beginFrame();
    glim::paint::GroupParams clipP;
    clipP.clip = glim::Rect{{10, 10}, {20, 20}};
    clipped.pushGroup(clipP);
    clipped.slot(glim::Rect{{0, 0}, {100, 100}}, 4);
    clipped.popGroup();
    clipped.finish();
    placed.clear();
    glim::paint::collectSlots(clipped.scene(), &placed);
    expect(placed.size() == 1, "clipped slot still collected");
    expect(placed.size() == 1 && glim::nearlyEqual(placed[0].windowLogical.origin.x, 10.f),
           "clip intersects origin x");
    expect(placed.size() == 1 && glim::nearlyEqual(placed[0].windowLogical.size.x, 20.f),
           "clip intersects width");

    glim::paint::Scene isolated;
    isolated.logicalSize = {50, 50};
    glim::paint::Group iso;
    iso.params.opacity = 0.5f;
    iso.shapes.emplace_back(glim::paint::SlotHole{glim::Rect::fromSize({10, 10}), 9});
    isolated.root.children.push_back(std::make_unique<glim::paint::Group>(std::move(iso)));
    placed.clear();
    glim::paint::collectSlots(isolated, &placed);
    expect(placed.empty(), "SlotHole under isolate is not collected");

    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(glim::Rect::fromSize({200, 100}));
    ctx.slot(glim::Rect{{20, 20}, {40, 40}}, 1);
    ctx.finish();
    const glim::paint::FramePacket packet = glim::paint::encode(ctx.scene());
    expect(packet.quads.size() == 1, "SlotHole emits no coverage");
    expect(packet.blits.empty(), "SlotHole is not a blit");

    const int w = 32;
    const int h = 24;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(w * h * 4), 0);
    glim::paint::Renderer renderer(buf.data(), w, h);
    ctx.setSize({static_cast<float>(w), static_cast<float>(h)});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(glim::Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
    ctx.slot(glim::Rect{{4, 4}, {8, 8}}, 2);
    ctx.finish();
    renderer.draw(ctx.scene());
    const std::size_t inside = static_cast<std::size_t>((6 * w + 6) * 4);
    expect(buf[inside] > 200 && buf[inside + 1] < 20, "skip-paint is not dest-out");

    ctx.beginFrame();
    ctx.slot(glim::Rect::fromSize({10, 10}), 0);
    ctx.finish();
    expect(ctx.scene().root.shapes.empty(), "id 0 is not recorded");

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "slot_test ok\n";
    return EXIT_SUCCESS;
}
