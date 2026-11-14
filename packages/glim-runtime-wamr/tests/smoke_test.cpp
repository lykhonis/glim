#include "SmokeScene.h"

#include <glim/guest/FrameRecords.h>
#include <glim/guest_host/Session.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Software.h>
#include <glim/runtime_wamr/Wamr.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {

namespace t = glim::transport;
namespace h = glim::guest_host;
namespace p = glim::paint;
namespace w = glim::runtime_wamr;
namespace s = glim::smoke;

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

std::vector<std::byte> readModule(const char* path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error(std::string("cannot open ") + path);
  }
  std::vector<char> bytes{std::istreambuf_iterator<char>(in), {}};
  const auto* data = reinterpret_cast<const std::byte*>(bytes.data());
  return {data, data + bytes.size()};
}

std::vector<std::uint8_t> raster(const p::FramePacket& packet) {
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(s::kWidth * s::kHeight * 4), 0);
  p::rasterPacket(packet, s::kWidth, s::kHeight, rgba.data());
  return rgba;
}

t::Support support() {
  t::Support out;
  out.channels = {{.channel = s::kFrameChannel, .oldest = 1, .newest = 1}};
  return out;
}

void guestFrameMatchesGolden(w::Engine& engine, const std::vector<std::byte>& module) {
  w::Instance guest(engine, module, {});
  std::vector<std::vector<std::byte>> frames;
  h::Session session(
    guest,
    {.support = support(),
     .clock = [] { return std::uint64_t{7}; },
     .logger = [](std::uint32_t, std::string_view text) { std::cerr << "guest: " << text << '\n'; }
    },
    [&frames](const t::Message& m) { frames.emplace_back(m.payload.begin(), m.payload.end()); }
  );
  guest.attach(session);
  expect(session.start(), "session starts");
  expect(session.state() == h::Session::State::Ready, "handshake completes in wasm");
  session.tick(16);
  expect(frames.size() == 1, "guest sends one frame");
  if (frames.size() != 1) {
    return;
  }
  const auto decoded = glim::guest::readFrame(frames[0], p::ImageStore{});
  expect(decoded.has_value(), "frame decodes");
  const auto golden = p::encode(s::scene());
  expect(!golden.quads.empty() && !golden.isolates.empty(), "scene covers quads and isolates");
  expect(decoded && raster(*decoded) == raster(golden), "wasm frame renders the in-process golden");
}

void unattachedImportTraps(w::Engine& engine, const std::vector<std::byte>& module) {
  w::Instance guest(engine, module, {});
  bool trapped = false;
  try {
    guest.init(t::kProtocolVersion, t::kMinRingCapacity);
  } catch (const std::runtime_error&) {
    trapped = true;
  }
  expect(trapped, "an import before attach traps and surfaces");
}

void malformedModuleRejected(w::Engine& engine) {
  const std::vector<std::byte> junk(16, std::byte{0x5a});
  bool rejected = false;
  try {
    w::Instance guest(engine, junk, {});
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  expect(rejected, "malformed module is rejected");
}

// (module (import "wasi_snapshot_preview1" "fd_read" (func)))
void foreignImportRefused(w::Engine& engine) {
  std::vector<std::byte> module;
  const auto put = [&module](std::string_view bytes) {
    for (const char c : bytes) {
      module.push_back(static_cast<std::byte>(c));
    }
  };
  put({"\0asm\x01\0\0\0", 8});
  put({"\x01\x04\x01\x60\0\0", 6});
  put({"\x02\x22\x01\x16", 4});
  put("wasi_snapshot_preview1");
  put("\x07");
  put("fd_read");
  put({"\0\0", 2});
  bool refused = false;
  try {
    w::Instance guest(engine, module, {});
  } catch (const std::runtime_error& error) {
    refused = std::string_view(error.what()).find("fd_read") != std::string_view::npos;
  }
  expect(refused, "an import outside the guest ABI is refused at load");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: glim-runtime-wamr-smoke <guest.wasm>\n";
    return 2;
  }
  const auto module = readModule(argv[1]);
  w::Engine engine;
  guestFrameMatchesGolden(engine, module);
  unattachedImportTraps(engine, module);
  malformedModuleRejected(engine);
  foreignImportRefused(engine);
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "glim-runtime-wamr smoke: ok\n";
  return 0;
}
