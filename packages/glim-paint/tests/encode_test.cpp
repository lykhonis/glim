#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>

#include <cstdlib>
#include <iostream>

int main() {
    glim::paint::Context ctx;
    ctx.setSize({720, 480});
    ctx.beginFrame();
    ctx.setFillColor(0x334c4cff);
    ctx.fill(glim::Rect::fromSize({720, 480}));
    ctx.translate({100, 50});
    ctx.setFillColor(0xac6363ff);
    ctx.fill(glim::Rect::fromSize({400, 300}));
    ctx.finish();

    const glim::paint::FramePacket packet = glim::paint::encode(ctx.scene());
    if (packet.quads.size() != 2) {
        std::cerr << "expected 2 quads, got " << packet.quads.size() << '\n';
        return EXIT_FAILURE;
    }
    if (!packet.isolates.empty()) {
        std::cerr << "opaque hello should not isolate\n";
        return EXIT_FAILURE;
    }

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
    const glim::paint::FramePacket iso = glim::paint::encode(glass.scene());
    if (iso.isolates.size() != 1) {
        std::cerr << "expected 1 isolate\n";
        return EXIT_FAILURE;
    }
    std::cout << "encode_test ok\n";
    return EXIT_SUCCESS;
}
