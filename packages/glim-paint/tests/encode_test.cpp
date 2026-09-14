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

    glim::paint::Context round;
    round.setSize({64, 64});
    round.beginFrame();
    round.setFillColor(0xff0000ff);
    round.fillRounded(glim::Rect::fromSize({40, 40}), glim::Radius{8.f});
    round.finish();
    const glim::paint::FramePacket strips = glim::paint::encode(round.scene());
    if (strips.quads.size() < 4) {
        std::cerr << "rounded fill should flatten to strips, got " << strips.quads.size() << '\n';
        return EXIT_FAILURE;
    }
    if (!strips.isolates.empty()) {
        std::cerr << "rounded fill should not isolate\n";
        return EXIT_FAILURE;
    }

    glim::paint::Context clip;
    clip.setSize({100, 40});
    clip.beginFrame();
    clip.setFillColor(0xff0000ff);
    clip.fill(glim::Rect::fromSize({100, 40}));
    glim::paint::GroupParams p;
    p.clip = glim::Rect{{10, 0}, {40, 40}};
    clip.pushGroup(p);
    clip.setFillColor(0x00ff00ff);
    clip.fill(glim::Rect{{0, 0}, {80, 40}});
    clip.popGroup();
    clip.finish();
    const glim::paint::FramePacket clipped = glim::paint::encode(clip.scene());
    if (!clipped.isolates.empty()) {
        std::cerr << "clip-only group should not isolate\n";
        return EXIT_FAILURE;
    }
    if (clipped.quads.size() < 2) {
        std::cerr << "clip encode should keep background and clipped content\n";
        return EXIT_FAILURE;
    }

    std::cout << "encode_test ok\n";
    return EXIT_SUCCESS;
}
