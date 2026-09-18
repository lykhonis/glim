#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Renderer.h>
#include <glim/paint/Scene.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
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
    using glim::Rect;
    using glim::paint::Glass;
    using glim::paint::GlassVariant;
    using glim::paint::GroupParams;

    {
        glim::paint::Context ctx;
        ctx.setSize({100, 100});
        ctx.beginFrame();
        ctx.setFillColor(0xff0000ff);
        ctx.fill(Rect::fromSize({100, 100}));
        GroupParams p;
        p.glass = Glass{};
        p.bounds = Rect{{10, 10}, {40, 20}};
        p.clipRadius = glim::Radius{10.f};
        ctx.pushGroup(p);
        ctx.popGroup();
        ctx.finish();
        glim::paint::Stats stats{};
        glim::paint::Group merged = glim::paint::merge(std::move(ctx.scene().root), &stats);
        expect(merged.children.size() == 1, "glass group stays isolated");
        expect(merged.children.size() == 1 && glim::paint::hasGlass(*merged.children[0]), "hasGlass");
        expect(merged.children.size() == 1 && glim::paint::hasGlassWork(*merged.children[0]),
               "hasGlassWork");
        expect(merged.children.size() == 1 && !glim::paint::hasBackdrop(*merged.children[0]),
               "glass is not backdrop");
        expect(merged.children.size() == 1 && glim::paint::needsIsolate(*merged.children[0]),
               "glass isolates");
        expect(merged.children.size() == 1 && !glim::paint::canMerge(*merged.children[0]),
               "canMerge refuses glass");
    }

    {
        glim::paint::Context ctx;
        ctx.setSize({80, 40});
        ctx.beginFrame();
        ctx.setFillColor(0xff0000ff);
        ctx.fill(Rect::fromSize({80, 40}));
        GroupParams p;
        p.glass = Glass{};
        p.bounds = Rect{{8, 4}, {32, 16}};
        p.clipRadius = glim::Radius{8.f};
        ctx.pushGroup(p);
        ctx.popGroup();
        ctx.finish();
        const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
        expect(pkt.isolates.size() >= 1, "glass encode isolate");
        bool found = false;
        for (const auto& iso : pkt.isolates) {
            if (!iso.hasGlass) {
                continue;
            }
            found = true;
            expect(iso.backdropSigma == 0.f, "independence: backdropSigma 0");
            expect(iso.backdropBend == 0.f, "independence: backdropBend 0");
            expect(!iso.glassPills.empty(), "glass isolate carries pills");
            expect(iso.glassU1 > iso.glassU0 && iso.glassV1 > iso.glassV0, "glass dest UV");
            expect(iso.contentW > 32, "guttered glassSurface wider than bounds");
            expect(iso.contentH > 16, "guttered glassSurface taller than bounds");
        }
        expect(found, "found glass isolate");
        expect(pkt.stats.glassPassCount >= 1, "encode glassPassCount");
        expect(pkt.stats.backdropCount == 0, "independence: no backdrop pyramid");
    }

    {
        glim::paint::Context ctx;
        ctx.setSize({80, 40});
        ctx.beginFrame();
        ctx.setFillColor(0xff0000ff);
        ctx.fill(Rect::fromSize({80, 40}));
        GroupParams frost;
        frost.backdropBlur = 16.f;
        frost.bounds = Rect{{4, 4}, {20, 12}};
        ctx.pushGroup(frost);
        ctx.popGroup();
        GroupParams g;
        g.glass = Glass{};
        g.bounds = Rect{{40, 4}, {20, 12}};
        ctx.pushGroup(g);
        ctx.popGroup();
        ctx.finish();
        const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
        expect(pkt.stats.backdropCount == 1, "both: backdropCount");
        expect(pkt.stats.glassPassCount == 1, "both: glassPassCount");
        int nIso = 0;
        for (const auto& iso : pkt.isolates) {
            if (iso.hasGlass || iso.backdropSigma > 0.f) {
                ++nIso;
            }
        }
        expect(nIso == 2, "both: two effect isolates");
    }

    {
        GroupParams empty;
        empty.glassContainer = true;
        empty.bounds = Rect{{0, 0}, {40, 20}};
        glim::paint::Group g;
        g.params = empty;
        expect(!glim::paint::hasGlassWork(g), "empty container has no work");
        expect(!glim::paint::needsIsolate(g), "empty container does not isolate");
    }

    {
        glim::paint::Context ctx;
        ctx.setSize({100, 40});
        ctx.beginFrame();
        ctx.setFillColor(0xff0000ff);
        ctx.fill(Rect::fromSize({100, 40}));
        GroupParams box;
        box.glassContainer = true;
        box.bounds = Rect{{4, 4}, {90, 24}};
        ctx.pushGroup(box);
        GroupParams a;
        a.glass = Glass{};
        a.bounds = Rect{{8, 6}, {24, 16}};
        a.clipRadius = glim::Radius{8.f};
        ctx.pushGroup(a);
        ctx.popGroup();
        GroupParams b;
        b.glass = Glass{};
        b.bounds = Rect{{40, 6}, {24, 16}};
        b.clipRadius = glim::Radius{8.f};
        ctx.pushGroup(b);
        ctx.popGroup();
        ctx.popGroup();
        ctx.finish();
        const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
        expect(pkt.stats.glassPassCount == 1, "container: one glass pass");
        bool found = false;
        for (const auto& iso : pkt.isolates) {
            if (!iso.hasGlass) {
                continue;
            }
            found = true;
            expect(iso.glassPills.size() == 2, "bound-only children emit two pills");
            expect(iso.isolates.empty() || !iso.isolates[0].hasGlass, "inner glass flags cleared");
        }
        expect(found, "container produced a glass isolate");
        glim::paint::GlassPill pills[glim::paint::kMaxGlassPills];
        glim::paint::Group merged = glim::paint::merge(ctx.scene().root, nullptr);
        expect(merged.children.size() == 1, "container stays a child");
        const int n = glim::paint::collectGlassPills(*merged.children[0], pills);
        expect(n == 2, "collectGlassPills bound-only fallback");
    }

    {
        glim::paint::Context ctx;
        ctx.setSize({80, 40});
        ctx.beginFrame();
        ctx.setFillColor(0x00ff00ff);
        ctx.fill(Rect::fromSize({80, 40}));
        GroupParams g;
        g.glass = Glass{};
        g.bounds = Rect{{8, 4}, {32, 16}};
        ctx.pushGroup(g);
        ctx.popGroup();
        ctx.setFillColor(0xffffffff);
        ctx.fill(Rect{{50, 4}, {10, 10}});
        ctx.finish();
        const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
        expect(pkt.stats.glassPassCount == 1, "merge lock: still one glass pass");
        expect(pkt.isolates.size() >= 2, "opaque sibling after glass is not folded ahead");
    }

    {
        glim::paint::Context ctx;
        ctx.setSize({80, 40});
        ctx.beginFrame();
        ctx.setFillColor(0xff0000ff);
        ctx.fill(Rect::fromSize({80, 40}));
        GroupParams outer;
        outer.glass = Glass{};
        outer.bounds = Rect{{8, 4}, {48, 24}};
        ctx.pushGroup(outer);
        GroupParams inner;
        inner.glass = Glass{};
        inner.bounds = Rect{{12, 8}, {16, 12}};
        ctx.pushGroup(inner);
        ctx.popGroup();
        ctx.popGroup();
        ctx.finish();
        const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
        expect(pkt.stats.glassPassCount == 1, "nested glass skipped inner snapshot");
    }

    {
        GroupParams p;
        p.glass = Glass{};
        p.glass->variant = GlassVariant::Identity;
        glim::paint::Group g;
        g.params = p;
        expect(glim::paint::hasGlass(g), "Identity is glass");
        expect(glim::paint::needsIsolate(g), "Identity isolates");
        expect(!glim::paint::hasBackdrop(g), "Identity is not backdrop");
    }

    {
        const int w = 64;
        const int h = 48;
        std::vector<std::uint8_t> buf(static_cast<std::size_t>(w * h * 4), 0);
        glim::paint::Renderer renderer(buf.data(), w, h);
        glim::paint::Context ctx;
        ctx.setSize({static_cast<float>(w), static_cast<float>(h)});
        ctx.beginFrame();
        ctx.setFillColor(0xff0000ff);
        ctx.fill(Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
        GroupParams p;
        p.glass = Glass{};
        p.bounds = Rect{{16.f, 12.f}, {32.f, 20.f}};
        p.clipRadius = glim::Radius{10.f};
        ctx.pushGroup(p);
        ctx.popGroup();
        ctx.finish();
        renderer.draw(ctx.scene());
        const std::size_t center = static_cast<std::size_t>((22 * w + 32) * 4);
        expect(buf[center] > 80, "cpu glass plate samples dest");
        expect(buf[center + 1] < 80 && buf[center + 2] < 80, "cpu glass plate is dest red, not warp black");
    }

    if (failures) {
        std::cerr << failures << " glass tests failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
