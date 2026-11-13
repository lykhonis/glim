#include "SmokeScene.h"

#include <glim/guest/FrameRecords.h>
#include <glim/guest/Link.h>
#include <glim/guest/Wasm.h>
#include <glim/paint/FramePacket.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace {

namespace t = glim::transport;

std::unique_ptr<std::uint64_t[]> region;
std::optional<glim::guest::Link> link;
bool drawn = false;

void draw() {
  std::vector<std::byte> frame;
  glim::guest::writeFrame(glim::paint::encode(glim::smoke::scene()), frame);
  drawn = link->endpoint()->post(t::Lane::Frame, glim::smoke::kFrameChannel, frame, true);
  link->flush();
}

}  // namespace

extern "C" __attribute__((export_name("glim_init"))) std::uint32_t glim_init(
    std::uint32_t protocol, std::uint32_t capacity
) {
  if (link) {
    return 0;
  }
  const auto bytes = t::linkBytes(capacity);
  region = std::make_unique<std::uint64_t[]>((bytes + 7) / 8);
  auto* at = reinterpret_cast<std::byte*>(region.get());
  t::Terms hello;
  hello.channels = {{.channel = glim::smoke::kFrameChannel, .version = 1, .required = true}};
  link.emplace(
      glim::guest::hostImports(),
      glim::guest::linearMemory,
      std::span(at, bytes),
      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(at)),
      std::move(hello),
      [](const t::Message&) {}
  );
  return link->init(protocol, capacity);
}

extern "C" __attribute__((export_name("glim_recv"))) void glim_recv() {
  if (link) {
    link->recv();
  }
}

extern "C" __attribute__((export_name("glim_tick"))) void glim_tick(std::uint64_t now) {
  if (!link) {
    return;
  }
  link->tick(now);
  if (!drawn && link->state() == glim::guest::Link::State::Ready) {
    draw();
  }
}
