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
    const glim::paint::FramePacket iso = glim::paint::encode(fade.scene());
    if (iso.isolates.size() != 1) {
        std::cerr << "expected 1 isolate\n";
        return EXIT_FAILURE;
    }
    const glim::paint::FramePacket iso2 = glim::paint::encode(fade.scene(), 2.f);
    if (iso2.isolates.size() != 1 || iso2.isolates[0].contentW != 200 || iso2.isolates[0].contentH != 40) {
        std::cerr << "isolate size should be ceil(bounds * pixelRatio)\n";
        return EXIT_FAILURE;
    }

    glim::paint::Context frost;
    frost.setSize({80, 40});
    frost.beginFrame();
    frost.setFillColor(0xff0000ff);
    frost.fill(glim::Rect::fromSize({80, 40}));
    glim::paint::GroupParams blur;
    blur.backdropBlur = 16.f;
    blur.bounds = glim::Rect{{8, 4}, {32, 16}};
    frost.pushGroup(blur);
    frost.setFillColor(0xffffff66);
    frost.fill(glim::Rect{{8, 4}, {32, 16}});
    frost.popGroup();
    frost.finish();
    const glim::paint::FramePacket frostPkt = glim::paint::encode(frost.scene(), 1.f);
    if (frostPkt.isolates.size() != 1 || frostPkt.isolates[0].backdropSigma != 16.f) {
        std::cerr << "backdrop isolate should carry snapped sigma\n";
        return EXIT_FAILURE;
    }
    if (frostPkt.stats.backdropCount != 1) {
        std::cerr << "expected one backdrop pyramid\n";
        return EXIT_FAILURE;
    }

    glim::paint::Context lens;
    lens.setSize({80, 40});
    lens.beginFrame();
    lens.setFillColor(0xff0000ff);
    lens.fill(glim::Rect::fromSize({80, 40}));
    glim::paint::GroupParams bend;
    bend.backdropBend = 0.5f;
    bend.clipRadius = glim::Radius{8.f};
    bend.bounds = glim::Rect{{8, 4}, {32, 16}};
    lens.pushGroup(bend);
    lens.popGroup();
    lens.finish();
    const glim::paint::FramePacket lensPkt = glim::paint::encode(lens.scene(), 1.f);
    if (lensPkt.isolates.size() != 1 || lensPkt.isolates[0].backdropBend != 0.5f ||
        lensPkt.isolates[0].backdropRadius != 8.f) {
        std::cerr << "bend isolate should carry radius and bend\n";
        return EXIT_FAILURE;
    }
    const auto& li = lensPkt.isolates[0];
    if (li.backdropPills.size() != 1 || li.backdropU1 <= li.backdropU0 || li.backdropV1 <= li.backdropV0) {
        std::cerr << "bend isolate should map dest UV and carry a pill\n";
        return EXIT_FAILURE;
    }

    glim::paint::Context merge;
    merge.setSize({80, 40});
    merge.beginFrame();
    merge.setFillColor(0xff0000ff);
    merge.fill(glim::Rect::fromSize({80, 40}));
    glim::paint::GroupParams blob;
    blob.backdropBend = 0.5f;
    blob.backdropMerge = 18.f;
    blob.bounds = glim::Rect{{8, 8}, {56, 20}};
    merge.pushGroup(blob);
    merge.fillRounded(glim::Rect{{8, 8}, {20, 20}}, glim::Radius{10.f});
    merge.fillRounded(glim::Rect{{36, 8}, {20, 20}}, glim::Radius{10.f});
    merge.popGroup();
    merge.finish();
    const glim::paint::FramePacket mergePkt = glim::paint::encode(merge.scene(), 1.f);
    if (mergePkt.isolates.size() != 1 || mergePkt.isolates[0].backdropPills.size() != 2 ||
        mergePkt.isolates[0].backdropMerge != 18.f) {
        std::cerr << "merge isolate should keep both pills\n";
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

    glim::paint::Context slot;
    slot.setSize({40, 40});
    slot.beginFrame();
    slot.setFillColor(0xff0000ff);
    slot.fill(glim::Rect::fromSize({40, 40}));
    slot.slot(glim::Rect{{4, 4}, {8, 8}}, 1);
    slot.finish();
    const glim::paint::FramePacket hole = glim::paint::encode(slot.scene());
    if (hole.quads.size() != 1) {
        std::cerr << "SlotHole should not encode coverage, got " << hole.quads.size() << " quads\n";
        return EXIT_FAILURE;
    }
    if (!hole.isolates.empty()) {
        std::cerr << "SlotHole should not isolate\n";
        return EXIT_FAILURE;
    }

    glim::paint::Context pills;
    pills.setSize({120, 40});
    pills.beginFrame();
    pills.setFillColor(0xff0000ff);
    pills.fill(glim::Rect::fromSize({120, 40}));
    glim::paint::GroupParams plate;
    plate.backdropBlur = 8.f;
    plate.bounds = glim::Rect{{4, 4}, {112, 32}};
    pills.pushGroup(plate);
    for (int i = 0; i < 6; ++i) {
        pills.fillRounded(glim::Rect{{8.f + static_cast<float>(i) * 18.f, 8.f}, {16, 16}},
                          glim::Radius{8.f});
    }
    pills.popGroup();
    pills.finish();
    const glim::paint::FramePacket pillPkt = glim::paint::encode(pills.scene(), 1.f);
    if (pillPkt.isolates.size() != 1 || pillPkt.isolates[0].backdropPills.size() != 4) {
        std::cerr << "backdrop pills cap at 4\n";
        return EXIT_FAILURE;
    }
    if (pillPkt.isolates[0].quads.empty()) {
        std::cerr << "rounded fills beyond the pill cap must survive as content\n";
        return EXIT_FAILURE;
    }

    std::cout << "encode_test ok\n";
    return EXIT_SUCCESS;
}
