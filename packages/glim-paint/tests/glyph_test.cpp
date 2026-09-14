#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Renderer.h>
#include <glim/paint/Scene.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
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
    ctx.setSize({128, 64});
    const glim::paint::TextSize empty = ctx.measureText("", 16.f);
    expect(empty.width == 0.f, "empty string width");
    expect(empty.ascent > 0.f, "ascent at 16px");

    const glim::paint::TextSize fps = ctx.measureText("FPS 60", 16.f);
    expect(fps.width > empty.width, "FPS 60 is wider than empty");
    const glim::paint::TextSize skip = ctx.measureText("A\x80""B", 16.f);
    const glim::paint::TextSize ab = ctx.measureText("AB", 16.f);
    expect(glim::nearlyEqual(skip.width, ab.width), "non-latin byte skipped");

    ctx.beginFrame();
    ctx.setFillColor(0x000000ff);
    ctx.fill(glim::Rect::fromSize({128.f, 64.f}));
    ctx.setFillColor(0xffffffff);
    ctx.text({8.f, 40.f}, "Hi", 32.f);
    ctx.finish();

    expect(ctx.scene().root.shapes.size() >= 2, "fill + glyph run");
    const glim::paint::GlyphRun* run = nullptr;
    for (const glim::paint::Shape& s : ctx.scene().root.shapes) {
        run = std::get_if<glim::paint::GlyphRun>(&s);
        if (run) {
            break;
        }
    }
    expect(run != nullptr, "recorded GlyphRun");
    expect(run && run->glyphs.size() >= 2, "H and i quads");
    expect(run && run->imageId != 0, "atlas imageId");

    glim::paint::Stats stats{};
    glim::paint::Group rootCopy = ctx.scene().root;
    glim::paint::Group merged = glim::paint::merge(std::move(rootCopy), &stats);
    expect(std::get_if<glim::paint::GlyphRun>(&merged.shapes.back()) != nullptr,
           "merged GlyphRun stays a GlyphRun");

    ctx.beginFrame();
    ctx.setFillColor(0x000000ff);
    ctx.fill(glim::Rect::fromSize({128.f, 64.f}));
    ctx.setFillColor(0xffffffff);
    ctx.text({8.f, 40.f}, "Hi", 32.f);
    ctx.finish();
    const glim::paint::FramePacket packet = glim::paint::encode(ctx.scene());
    expect(!packet.blits.empty(), "encode expands glyphs");
    bool sdf = false;
    for (const glim::paint::BlitQuad& q : packet.blits) {
        if (q.sdf) {
            sdf = true;
        }
    }
    expect(sdf, "encoded glyph quads are SDF");

    const int w = 128;
    const int h = 64;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(w * h * 4), 0);
    glim::paint::Renderer renderer(buf.data(), w, h);
    renderer.draw(ctx.scene());
    bool lit = false;
    for (int y = 10; y < 50 && !lit; ++y) {
        for (int x = 8; x < 60; ++x) {
            const std::size_t i = static_cast<std::size_t>((y * w + x) * 4);
            if (buf[i] > 80 && buf[i + 1] > 80 && buf[i + 2] > 80) {
                lit = true;
            }
        }
    }
    expect(lit, "CPU SDF coverage on Hi");
    const std::size_t far = static_cast<std::size_t>((4 * w + 120) * 4);
    expect(buf[far] < 20 && buf[far + 1] < 20 && buf[far + 2] < 20, "far pixel stays dark");

    if (failures != 0) {
        std::cerr << failures << " glyph tests failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "glyph_test ok\n";
    return EXIT_SUCCESS;
}
