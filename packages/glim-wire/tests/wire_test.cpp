#include <glim/wire/Validate.h>
#include <glim/wire/Writer.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

namespace w = glim::wire;

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

// Test schema: Label v1 = {u32 id @8, f32 x @12, name @16}, fixed 24;
// v2 appends {f32 opacity @24}, fixed 32. Points = {xy pairs @8}.
constexpr std::uint16_t kLabel = 1;
constexpr std::uint16_t kPoints = 2;
constexpr std::uint32_t kId = 8, kX = 12, kName = 16, kOpacity = 24;
constexpr std::uint32_t kLabelV1 = 24, kLabelV2 = 32;
constexpr std::uint32_t kPointsRef = 8, kPointsFixed = 16;

constexpr w::TailRule kLabelTails[] = {{kName, 1}};
constexpr w::TailRule kPointsTails[] = {{kPointsRef, 8}};

bool finiteX(w::RecordView r) { return std::isfinite(r.field<float>(kX)); }

constexpr w::TypeRule kRules[] = {{kLabel, kLabelTails, finiteX}, {kPoints, kPointsTails}};

std::string_view text(std::span<const std::byte> bytes) {
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

std::vector<std::byte> label(std::uint32_t fixed, std::string_view name, float x = 2.5f) {
  std::vector<std::byte> out;
  w::Writer writer(out);
  auto b = writer.begin(kLabel, fixed);
  b.set(kId, 7u).set(kX, x).tail(kName, name);
  if (fixed >= kLabelV2) {
    b.set(kOpacity, 0.5f);
  }
  return out;
}

void endian() {
  std::byte raw[8];
  w::storeLe(raw, std::uint32_t{0x01020304});
  expect(raw[0] == std::byte{4} && raw[3] == std::byte{1}, "store is little-endian");
  expect(w::loadLe<std::uint32_t>(raw) == 0x01020304, "u32 round-trip");
  w::storeLe(raw, -1.25);
  expect(w::loadLe<double>(raw) == -1.25, "f64 round-trip");
  w::storeLe(raw, std::int16_t{-2});
  expect(w::loadLe<std::int16_t>(raw) == -2, "i16 round-trip");
}

void roundTrip() {
  auto msg = label(kLabelV2, "hello");
  expect(msg.size() % 8 == 0 && msg.size() == 40, "record padded to 8");
  expect(w::validate(msg, kRules) == w::Status::Ok, "valid record");
  w::RecordView r(msg.data());
  expect(r.type() == kLabel && r.fixedSize() == kLabelV2 && r.size() == 40, "header");
  expect(r.field<std::uint32_t>(kId) == 7 && r.field<float>(kX) == 2.5f, "fields");
  expect(r.field<float>(kOpacity, 1.f) == 0.5f, "appended field");
  expect(text(*r.tail(kName)) == "hello", "tail");
}

void versioning() {
  auto older = label(kLabelV1, "old");
  w::RecordView r(older.data());
  expect(w::validate(older, kRules) == w::Status::Ok, "older record valid");
  expect(
    r.field<float>(kOpacity, 1.f) == 1.f, "newer reader defaults missing field over tail bytes"
  );
  expect(text(*r.tail(kName)) == "old", "older tail");

  std::vector<std::byte> newer;
  w::Writer(newer)
    .begin(kLabel, 48)
    .set(kId, 9u)
    .set(40, 123u)
    .tail(kName, std::string_view("new"));
  w::RecordView n(newer.data());
  expect(w::validate(newer, kRules) == w::Status::Ok, "newer record valid");
  expect(
    n.field<std::uint32_t>(kId) == 9 && text(*n.tail(kName)) == "new",
    "older reader ignores extra fields"
  );

  std::vector<std::byte> bare;
  w::Writer(bare).begin(kLabel, 8);
  expect(w::validate(bare, kRules) == w::Status::Ok, "header-only record valid");
  expect(w::RecordView(bare.data()).tail(kName)->empty(), "absent tail ref is empty");
}

void unknownTypes() {
  std::vector<std::byte> msg;
  w::Writer writer(msg);
  writer.begin(900, 16).set(8, 1u);
  writer.begin(kLabel, kLabelV1).set(kId, 3u).tail(kName, std::string_view("x"));
  writer.begin(kPoints, kPointsFixed)
    .tail(kPointsRef, std::as_bytes(std::span<const float>({1.f, 2.f, 3.f, 4.f})));
  expect(w::validate(msg, kRules) == w::Status::Ok, "unknown type skipped");
  int seen = 0;
  w::forEachKnown(msg, kRules, [&](w::RecordView r) {
    seen += r.type();
    if (r.type() == kPoints) {
      expect(r.tail(kPointsRef)->size() == 16, "points tail");
    }
  });
  expect(seen == kLabel + kPoints, "forEachKnown visits known records");

  std::vector<std::byte> must;
  w::Writer(must).begin(900, 8, w::kMustUnderstand);
  expect(
    w::validate(must, kRules) == w::Status::MustUnderstand, "must-understand unknown rejected"
  );
  std::vector<std::byte> knownMust;
  w::Writer(knownMust).begin(kLabel, 8, w::kMustUnderstand);
  expect(w::validate(knownMust, kRules) == w::Status::Ok, "must-understand known accepted");
}

void malformed() {
  auto good = label(kLabelV1, "abc");
  auto patch = [&](std::uint32_t at, auto value) {
    auto copy = good;
    w::storeLe(copy.data() + at, value);
    return w::validate(copy, kRules);
  };
  expect(w::validate(std::span(good).first(4), kRules) == w::Status::Truncated, "short header");
  expect(
    w::validate(std::span(good).first(good.size() - 8), kRules) == w::Status::Truncated,
    "truncated record"
  );
  expect(patch(w::kSizeOffset, 4u) == w::Status::BadSize, "size below header");
  expect(patch(w::kSizeOffset, 33u) == w::Status::BadSize, "unaligned size");
  expect(patch(w::kSizeOffset, 0xfffffff8u) == w::Status::Truncated, "size past buffer");
  expect(patch(w::kWordsOffset, std::uint8_t{0}) == w::Status::BadFixed, "zero fixed part");
  expect(patch(w::kWordsOffset, std::uint8_t{5}) == w::Status::BadFixed, "fixed past size");
  expect(patch(kName, 8u) == w::Status::BadTail, "tail inside fixed part");
  expect(patch(kName + 4, 0xffffffffu) == w::Status::BadTail, "tail length overflow");
  expect(patch(kName, 0xfffffff0u) == w::Status::BadTail, "tail offset overflow");
  expect(patch(kX, std::numeric_limits<float>::quiet_NaN()) == w::Status::Rejected, "value check");

  std::vector<std::byte> points;
  w::Writer(points).begin(kPoints, kPointsFixed).tail(kPointsRef, std::string_view("12345"));
  expect(w::validate(points, kRules) == w::Status::BadTail, "tail not a whole count of elements");

  expect(w::validate({}, kRules) == w::Status::Ok, "empty message");
}

void writerGuards() {
  std::vector<std::byte> out;
  w::Writer writer(out);
  bool threw = false;
  try {
    writer.begin(kLabel, 16).set(16, 1u);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  expect(threw, "field past fixed part throws");
  threw = false;
  try {
    writer.begin(kLabel, w::kMaxFixedSize + 1);
  } catch (const std::length_error&) {
    threw = true;
  }
  expect(threw, "oversized fixed part throws");
}

}  // namespace

int main() {
  endian();
  roundTrip();
  versioning();
  unknownTypes();
  malformed();
  writerGuards();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "glim-wire: ok\n";
  return 0;
}
