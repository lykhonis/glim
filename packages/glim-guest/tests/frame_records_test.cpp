#include <glim/guest/FrameRecords.h>
#include <glim/paint/Context.h>
#include <glim/paint/Software.h>
#include <glim/wire/Writer.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

namespace {

namespace g = glim::guest;
namespace p = glim::paint;
namespace w = glim::wire;

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

constexpr int kW = 96;
constexpr int kH = 72;

p::Scene scene() {
  p::Context ctx;
  ctx.setSize({kW, kH});
  ctx.beginFrame();
  ctx.setFillColor(0x2050a0ff);
  ctx.fill(glim::Rect::fromSize({kW, kH}));

  const p::GradientStop stops[] = {{0.f, glim::Color{0xff8000ff}}, {1.f, glim::Color{0x00c0ffff}}};
  ctx.setFill(p::Matter::linearGradient({0, 0}, {kW, 0}, stops, 2));
  ctx.fillRounded(glim::Rect{{4, 4}, {88, 20}}, glim::Radius{6.f});

  std::vector<std::uint8_t> checker(8 * 8 * 4);
  for (int i = 0; i < 64; ++i) {
    const std::uint8_t v = ((i / 8 + i % 8) % 2) ? 255 : 40;
    checker[i * 4 + 0] = v;
    checker[i * 4 + 1] = 255 - v;
    checker[i * 4 + 2] = v;
    checker[i * 4 + 3] = 255;
  }
  ctx.blit(glim::Rect{{8, 30}, {24, 24}}, ctx.addImage(8, 8, checker.data()));
  ctx.text({40, 40}, "Glim", 14.f);

  p::GroupParams fade;
  fade.opacity = 0.5f;
  fade.bounds = glim::Rect{{60, 28}, {30, 30}};
  fade.shadow = p::ShadowBlur{4.f, {2, 2}, glim::Color{0x00000080}, 0.f};
  ctx.pushGroup(fade);
  ctx.setFillColor(0x30ff60ff);
  ctx.fillRounded(glim::Rect{{60, 28}, {30, 30}}, glim::Radius{8.f});
  ctx.popGroup();

  p::GroupParams glass;
  glass.lens = p::Lens{};
  glass.bounds = glim::Rect{{20, 48}, {56, 20}};
  glass.clipRadius = glim::Radius{10.f};
  ctx.pushGroup(glass);
  ctx.popGroup();
  ctx.finish();
  return ctx.scene();
}

std::vector<std::uint8_t> raster(const p::FramePacket& packet) {
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kW * kH * 4), 0);
  p::rasterPacket(packet, kW, kH, rgba.data());
  return rgba;
}

std::vector<std::byte> encoded(const p::FramePacket& packet) {
  std::vector<std::byte> out;
  g::writeFrame(packet, out);
  return out;
}

void goldenEquality() {
  const p::Scene s = scene();
  const p::FramePacket direct = p::encode(s);
  expect(
    !direct.quads.empty() && !direct.blits.empty() && !direct.gradients.empty(),
    "scene covers draw kinds"
  );
  expect(!direct.isolates.empty(), "scene carries isolates");

  const auto bytes = encoded(direct);
  const auto decoded = g::readFrame(bytes, direct.images);
  expect(decoded.has_value(), "frame decodes");
  if (!decoded) {
    return;
  }
  expect(raster(*decoded) == raster(direct), "decoded packet renders the in-process golden");
  expect(encoded(*decoded) == bytes, "re-encode is byte-identical");
  expect(
    decoded->logicalSize.x == kW && decoded->isolates.size() == direct.isolates.size(),
    "frame fields"
  );
}

std::optional<p::FramePacket> decode(const std::vector<std::byte>& bytes) {
  return g::readFrame(bytes, p::ImageStore{});
}

p::FramePacket small() {
  p::FramePacket packet;
  packet.logicalSize = {10, 10};
  packet.quads.push_back(p::Quad{0, 0, 10, 10, 1, 0, 0, 1, 0});
  p::Isolate iso;
  iso.lensPills.push_back(p::LensPill{});
  packet.isolates.push_back(iso);
  return packet;
}

void structure() {
  const auto good = encoded(small());
  expect(decode(good).has_value(), "small frame decodes");

  expect(!decode({}).has_value(), "empty message has no frame");
  expect(!decode({std::byte{0xff}}).has_value(), "partial header is not walked");
  auto twice = good;
  twice.insert(twice.end(), good.begin(), good.end());
  expect(!decode(twice).has_value(), "two frame records rejected");

  std::vector<std::byte> unbalanced = good;
  w::Writer(unbalanced).begin(g::kIsolateEndRecord, 8, w::kMustUnderstand);
  expect(!decode(unbalanced).has_value(), "unmatched isolate end rejected");

  std::vector<std::byte> open = encoded(p::FramePacket{});
  w::Writer(open).begin(g::kIsolateBeginRecord, 8, w::kMustUnderstand);
  expect(!decode(open).has_value(), "unclosed isolate rejected");

  std::vector<std::byte> deep = encoded(p::FramePacket{});
  for (int i = 0; i <= g::kMaxIsolateDepth; ++i) {
    w::Writer(deep).begin(g::kIsolateBeginRecord, 8, w::kMustUnderstand);
  }
  for (int i = 0; i <= g::kMaxIsolateDepth; ++i) {
    w::Writer(deep).begin(g::kIsolateEndRecord, 8, w::kMustUnderstand);
  }
  expect(!decode(deep).has_value(), "isolate depth limit");

  std::vector<std::byte> rootPills = encoded(p::FramePacket{});
  w::Writer(rootPills)
    .begin(g::kLensPillsRecord, 24, w::kMustUnderstand)
    .set(8, 25u)
    .allocTail(16, 25);
  expect(!decode(rootPills).has_value(), "pills outside an isolate rejected");

  std::vector<std::byte> headless;
  w::Writer(headless).begin(g::kQuadsRecord, 24, w::kMustUnderstand).set(8, 33u).allocTail(16, 33);
  expect(!decode(headless).has_value(), "draws before the frame record rejected");
}

void values() {
  auto nan = small();
  nan.quads[0].x = std::numeric_limits<float>::quiet_NaN();
  expect(!decode(encoded(nan)).has_value(), "NaN rejected");

  auto badEnum = encoded(small());
  auto pills = small();
  pills.isolates[0].lensPills[0].kind = p::LensPillKind::Rounded;
  const auto rounded = encoded(pills);
  bool found = false;
  for (std::size_t i = 0; i < badEnum.size(); ++i) {
    if (badEnum[i] != rounded[i]) {
      badEnum[i] = std::byte{7};
      found = true;
    }
  }
  expect(found && !decode(badEnum).has_value(), "out-of-range enum rejected");

  auto content = small();
  content.isolates[0].contentW = 0;
  expect(!decode(encoded(content)).has_value(), "zero content size rejected");

  auto stops = small();
  stops.gradients.push_back(p::GradientQuad{});
  stops.gradients[0].stopCount = p::kMaxGradientStops + 1;
  expect(!decode(encoded(stops)).has_value(), "stop count rejected");

  auto many = small();
  many.isolates[0].lensPills.resize(p::kMaxLensPills + 1);
  expect(!decode(encoded(many)).has_value(), "lens pill count rejected");
}

void padding() {
  p::FramePacket packet;
  packet.quads.resize(1);
  std::memset(static_cast<void*>(packet.quads.data()), 0xab, sizeof(p::Quad));
  packet.quads[0] = p::Quad{1, 2, 3, 4, 1, 1, 1, 1, 1};
  std::memset(reinterpret_cast<std::byte*>(packet.quads.data()) + 33, 0xab, 3);
  const auto bytes = encoded(packet);
  const auto quad = bytes.end() - 40;
  expect(
    quad[32] == std::byte{1} && quad[33] == std::byte{0} && quad[34] == std::byte{0} &&
      quad[35] == std::byte{0},
    "struct padding is zeroed on the wire"
  );
  const auto back = decode(bytes);
  expect(
    back && back->quads.size() == 1 && back->quads[0].h == 4 && back->quads[0].plus == 1,
    "memcpy decode"
  );
}

void reuse() {
  const auto big = encoded(p::encode(scene()));
  const auto little = encoded(small());
  p::FramePacket packet;
  expect(
    g::readFrame(big, p::ImageStore{}, packet) && !packet.isolates.empty(), "decode into packet"
  );
  const auto capacity = packet.quads.capacity();
  expect(
    g::readFrame(little, p::ImageStore{}, packet) && packet.quads.size() == 1 &&
      packet.isolates.size() == 1,
    "second frame replaces the first"
  );
  expect(packet.quads.capacity() == capacity, "array capacity reused across frames");
  auto broken = little;
  broken.resize(broken.size() - 8);
  expect(
    !g::readFrame(broken, p::ImageStore{}, packet) && packet.quads.empty() &&
      packet.isolates.empty(),
    "failed decode leaves no draws"
  );
}

void versioning() {
  std::vector<std::byte> older;
  w::Writer writer(older);
  writer.begin(g::kFrameRecord, 16, w::kMustUnderstand).set(8, 10.f).set(12, 20.f);
  writer.begin(g::kQuadsRecord, 24, w::kMustUnderstand).set(8, 16u).allocTail(16, 16);
  w::storeLe(older.data() + 16 + 24, 5.f);
  const auto frame = decode(older);
  expect(
    frame && frame->logicalSize.y == 20.f && frame->dirtyRect.size.x == 0.f, "older frame defaults"
  );
  expect(
    frame && frame->quads.size() == 1 && frame->quads[0].x == 5.f && frame->quads[0].a == 0.f,
    "shorter element stride defaults later fields"
  );

  std::vector<std::byte> newer;
  w::Writer next(newer);
  next.begin(g::kFrameRecord, 64, w::kMustUnderstand).set(8, 10.f).set(56, 99u);
  auto quads = next.begin(g::kQuadsRecord, 24, w::kMustUnderstand);
  quads.set(8, 40u);
  auto tail = quads.allocTail(16, 80);
  w::storeLe(tail.data() + 40, 7.f);
  next.begin(0x7fff, 16);
  const auto ahead = decode(newer);
  expect(
    ahead && ahead->quads.size() == 2 && ahead->quads[1].x == 7.f,
    "longer stride and unknown types skipped"
  );

  std::vector<std::byte> required = encoded(small());
  w::Writer(required).begin(0x7fff, 8, w::kMustUnderstand);
  expect(!decode(required).has_value(), "unknown must-understand record rejects the frame");
}

}  // namespace

int main() {
  goldenEquality();
  structure();
  values();
  padding();
  reuse();
  versioning();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "glim-guest frame records: ok\n";
  return 0;
}
