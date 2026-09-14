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
    int dummy = 0;
    glim::paint::Context ctx;
    ctx.setSize({64, 48});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(glim::Rect::fromSize({64, 48}));
    const std::uint32_t id =
        ctx.wrapNativeTexture(&dummy, 8, 8, glim::paint::SampleFormat::Rgba8Unorm);
    expect(id != 0, "wrapNativeTexture returns id");
    ctx.blit(glim::Rect{{4, 4}, {16, 16}}, glim::paint::Matter::foreign(id));
    ctx.finish();

    expect(ctx.scene().root.shapes.size() == 2, "fill + foreign blit");
    const auto* blit = std::get_if<glim::paint::Blit>(&ctx.scene().root.shapes[1]);
    expect(blit && blit->matter.kind == glim::paint::MatterKind::Foreign, "MatterKind::Foreign");
    expect(blit && blit->matter.imageId == id, "foreign imageId");

    const glim::paint::StoredImage* img = ctx.images().get(id);
    expect(img && img->foreign && img->native == &dummy, "ImageStore foreign native");
    expect(img && img->rgba.empty(), "foreign has no CPU pixels");
    expect(img && img->width == 8 && img->height == 8, "foreign size");

    expect(ctx.wrapNativeTexture(nullptr, 8, 8) == 0, "reject null native");
    expect(ctx.wrapNativeTexture(&dummy, 0, 8) == 0, "reject empty");
    expect(ctx.wrapNativeTexture(&dummy, 5000, 8) == 0, "reject oversize side");

    const glim::paint::FramePacket packet = glim::paint::encode(ctx.scene());
    expect(packet.blits.size() == 1, "encode keeps foreign blit");
    expect(packet.blits.size() == 1 && packet.blits[0].imageId == id, "encoded imageId");

    std::vector<std::uint8_t> buf(static_cast<std::size_t>(64 * 48 * 4), 0);
    glim::paint::Renderer cpu(buf.data(), 64, 48);
    cpu.draw(ctx.scene());
    expect(buf[0] > 200 && buf[1] < 20 && buf[2] < 20, "cpu fill red");
    const std::size_t mid = static_cast<std::size_t>((8 * 64 + 8) * 4);
    expect(buf[mid] > 200 && buf[mid + 1] < 20 && buf[mid + 2] < 20,
           "cpu skips foreign blit (no CPU pixels)");

    glim::paint::Group merged = glim::paint::merge(ctx.scene().root);
    expect(merged.shapes.size() == 2, "foreign blit merges like sampled");

    ctx.releaseImage(id);
    expect(ctx.images().get(id) == nullptr, "release drops foreign");

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
