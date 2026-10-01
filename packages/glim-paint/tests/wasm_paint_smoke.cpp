#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Renderer.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

int main() {
    const int w = 64;
    const int h = 48;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(w * h * 4), 0);
    glim::paint::Renderer host(buf.data(), w, h);

    glim::paint::Context guest;
    guest.setSize({static_cast<float>(w), static_cast<float>(h)});
    guest.beginFrame();
    guest.setFillColor(0xff0000ff);
    guest.fill(glim::Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
    guest.setFillColor(0x00ff00ff);
    guest.fill(glim::Rect{{8.f, 8.f}, {16.f, 16.f}});
    guest.finish();

    const glim::paint::FramePacket packet = glim::paint::encode(guest.scene());
    if (packet.quads.empty()) {
        std::cerr << "guest encode produced no quads\n";
        return EXIT_FAILURE;
    }
    host.submit(packet);
    if (buf[0] < 200 || buf[1] > 20) {
        std::cerr << "host submit missed background\n";
        return EXIT_FAILURE;
    }
    const std::size_t inner = static_cast<std::size_t>((16 * w + 16) * 4);
    if (buf[inner + 1] < 200 || buf[inner] > 20) {
        std::cerr << "host submit missed foreground\n";
        return EXIT_FAILURE;
    }
    if (host.stats().instances == 0) {
        std::cerr << "host submit reported no instances\n";
        return EXIT_FAILURE;
    }

    std::fill(buf.begin(), buf.end(), 0);
    guest.beginFrame();
    guest.setFillColor(0xff0000ff);
    guest.fill(glim::Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
    glim::paint::GroupParams g;
    g.opacity = 0.5f;
    g.bounds = glim::Rect{{8.f, 8.f}, {16.f, 16.f}};
    guest.pushGroup(g);
    guest.setFillColor(0x00ff00ff);
    guest.fill(glim::Rect{{8.f, 8.f}, {16.f, 16.f}});
    guest.popGroup();
    guest.finish();
    const glim::paint::FramePacket iso = glim::paint::encode(guest.scene());
    if (iso.isolates.size() != 1) {
        std::cerr << "guest encode should carry one isolate\n";
        return EXIT_FAILURE;
    }
    if (iso.isolates[0].quads.size() != 1 || iso.isolates[0].quads[0].w != 16.f ||
        iso.isolates[0].quads[0].h != 16.f) {
        std::cerr << "isolate content should keep full bounds, got " << iso.isolates[0].quads.size()
                  << " quads\n";
        return EXIT_FAILURE;
    }
    host.submit(iso);
    if (buf[0] < 200) {
        std::cerr << "host submit lost background under isolate\n";
        return EXIT_FAILURE;
    }
    if (buf[inner] < 100 || buf[inner + 1] < 100) {
        std::cerr << "host submit missed isolate blend\n";
        return EXIT_FAILURE;
    }

    std::cout << "wasm_paint_smoke ok\n";
    return EXIT_SUCCESS;
}
