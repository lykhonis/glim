#include <glim/guest/FrameRecords.h>

#include <glim/wire/Validate.h>
#include <glim/wire/Writer.h>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace glim::guest {

namespace {

namespace w = glim::wire;
namespace p = glim::paint;

// Array record: header, u32 element stride, u32 reserved, tail ref.
constexpr std::uint32_t kStrideOffset = 8;
constexpr std::uint32_t kArrayTail = 16;
constexpr std::uint32_t kArrayFixed = 24;
constexpr int kMaxContentSide = 16384;

// Element field lists below follow the in-memory struct layout, padding
// included, so on little-endian hosts an array moves with one memcpy.
// Each list drives every Io: size, write, read, value checks, pad zeroing.

class Count {
 public:
  template <w::Scalar T>
  constexpr void scalar(const T&) {
    at_ += sizeof(T);
  }
  constexpr void bounded(const std::uint8_t&, std::uint8_t) { at_ += 1; }
  constexpr void boolean(const bool&) { at_ += 1; }
  template <class E>
  constexpr void enumeration(const E&, E) {
    at_ += 1;
  }
  constexpr void color(const Color&) { at_ += 4; }
  constexpr void pad(std::uint32_t n) { at_ += n; }
  constexpr std::uint32_t size() const { return at_; }

 private:
  std::uint32_t at_ = 0;
};

class Put {
 public:
  explicit Put(std::byte* out) : out_(out) {}

  template <w::Scalar T>
  void scalar(const T& v) {
    w::storeLe(out_ + at_, v);
    at_ += sizeof(T);
  }
  void bounded(const std::uint8_t& v, std::uint8_t) { scalar(v); }
  void boolean(const bool& b) { scalar(static_cast<std::uint8_t>(b ? 1 : 0)); }
  template <class E>
  void enumeration(const E& e, E) {
    scalar(static_cast<std::uint8_t>(e));
  }
  void color(const Color& c) { scalar(c.rgba); }
  void pad(std::uint32_t n) {
    std::memset(out_ + at_, 0, n);
    at_ += n;
  }

 private:
  std::byte* out_;
  std::uint32_t at_ = 0;
};

// Clears padding after a whole-array memcpy so no host memory leaks out.
class ZeroPads {
 public:
  explicit ZeroPads(std::byte* out) : out_(out) {}

  template <w::Scalar T>
  void scalar(const T&) {
    at_ += sizeof(T);
  }
  void bounded(const std::uint8_t&, std::uint8_t) { at_ += 1; }
  void boolean(const bool&) { at_ += 1; }
  template <class E>
  void enumeration(const E&, E) {
    at_ += 1;
  }
  void color(const Color&) { at_ += 4; }
  void pad(std::uint32_t n) {
    std::memset(out_ + at_, 0, n);
    at_ += n;
  }

 private:
  std::byte* out_;
  std::uint32_t at_ = 0;
};

// Fields past `limit` keep their defaults: an older sender omitted them.
class Get {
 public:
  Get(const std::byte* in, std::uint32_t limit) : in_(in), limit_(limit) {}

  template <w::Scalar T>
  void scalar(T& v) {
    if (at_ + sizeof(T) <= limit_) {
      v = w::loadLe<T>(in_ + at_);
    }
    at_ += sizeof(T);
  }
  void bounded(std::uint8_t& v, std::uint8_t) { scalar(v); }
  void boolean(bool& b) {
    std::uint8_t u = b ? 1 : 0;
    scalar(u);
    ok_ = ok_ && u <= 1;
    b = u == 1;
  }
  template <class E>
  void enumeration(E& e, E) {
    auto u = static_cast<std::uint8_t>(e);
    scalar(u);
    e = static_cast<E>(u);
  }
  void color(Color& c) { scalar(c.rgba); }
  void pad(std::uint32_t n) { at_ += n; }
  bool ok() const { return ok_; }

 private:
  const std::byte* in_;
  std::uint32_t limit_;
  std::uint32_t at_ = 0;
  bool ok_ = true;
};

// Value rules, run on the host's own copy (never on shared bytes).
class Check {
 public:
  template <w::Scalar T>
  void scalar(const T& v) {
    if constexpr (std::is_floating_point_v<T>) {
      ok_ = ok_ && std::isfinite(v);
    }
  }
  void bounded(const std::uint8_t& v, std::uint8_t max) { ok_ = ok_ && v <= max; }
  void boolean(const bool&) {}
  template <class E>
  void enumeration(const E& e, E max) {
    ok_ = ok_ && static_cast<std::uint8_t>(e) <= static_cast<std::uint8_t>(max);
  }
  void color(const Color&) {}
  void pad(std::uint32_t) {}
  bool ok() const { return ok_; }

 private:
  bool ok_ = true;
};

template <class Io, class V>
constexpr void vec2(Io& io, V& v) {
  io.scalar(v.x);
  io.scalar(v.y);
}

template <class Io, class R>
constexpr void rect(Io& io, R& r) {
  vec2(io, r.origin);
  vec2(io, r.size);
}

template <class Io, class Q>
constexpr void quadFields(Io& io, Q& q) {
  io.scalar(q.x);
  io.scalar(q.y);
  io.scalar(q.w);
  io.scalar(q.h);
  io.scalar(q.r);
  io.scalar(q.g);
  io.scalar(q.b);
  io.scalar(q.a);
  io.scalar(q.plus);
  io.pad(3);
}

template <class Io, class B>
constexpr void blitFields(Io& io, B& b) {
  io.scalar(b.x);
  io.scalar(b.y);
  io.scalar(b.w);
  io.scalar(b.h);
  io.scalar(b.u0);
  io.scalar(b.v0);
  io.scalar(b.u1);
  io.scalar(b.v1);
  io.scalar(b.r);
  io.scalar(b.g);
  io.scalar(b.b);
  io.scalar(b.a);
  io.scalar(b.imageId);
  io.scalar(b.sdf);
  io.scalar(b.plus);
  io.pad(2);
}

template <class Io, class G>
constexpr void gradientFields(Io& io, G& g) {
  io.scalar(g.x);
  io.scalar(g.y);
  io.scalar(g.w);
  io.scalar(g.h);
  io.scalar(g.coverage);
  io.bounded(g.kind, 1);
  io.pad(3);
  io.scalar(g.p0x);
  io.scalar(g.p0y);
  io.scalar(g.p1x);
  io.scalar(g.p1y);
  io.scalar(g.radius);
  io.scalar(g.plus);
  io.bounded(g.stopCount, p::kMaxGradientStops);
  io.pad(2);
  for (auto* channel : {&g.offsets, &g.r, &g.g, &g.b, &g.a}) {
    for (auto& v : *channel) {
      io.scalar(v);
    }
  }
}

template <class Io, class B>
constexpr void backdropPillFields(Io& io, B& b) {
  rect(io, b.rect);
  io.scalar(b.radius);
}

template <class Io, class L>
constexpr void lensPillFields(Io& io, L& l) {
  rect(io, l.rect);
  io.scalar(l.radius);
  io.scalar(l.superellipseN);
  io.enumeration(l.kind, p::LensPillKind::Rounded);
  io.pad(3);
}

template <class Io, class L>
void lensFields(Io& io, L& l) {
  io.enumeration(l.variant, p::LensVariant::Identity);
  io.enumeration(l.material, p::LensMaterial::Water);
  io.color(l.tint);
  io.boolean(l.interactive);
  io.scalar(l.ior);
  io.scalar(l.thicknessPx);
  io.scalar(l.mergeKPx);
  io.scalar(l.refDistance);
  io.scalar(l.dispersion);
  io.boolean(l.flatten);
  io.enumeration(l.edge, p::LensEdge::Full);
  io.scalar(l.transmit);
  io.scalar(l.rippleAmp);
  io.scalar(l.timeSec);
  io.scalar(l.envSoft);
  io.scalar(l.absorbK);
  io.scalar(l.swellAmp);
  vec2(io, l.swellScale);
  io.scalar(l.motion.period);
  io.scalar(l.motion.travel);
  io.scalar(l.motion.moveShare);
  io.enumeration(l.motion.curve, p::LensMotionCurve::Flow);
  io.scalar(l.motion.bounce);
  io.scalar(l.envLight);
}

template <class Io, class I>
void isolateFields(Io& io, I& i) {
  io.scalar(i.destX);
  io.scalar(i.destY);
  io.scalar(i.destW);
  io.scalar(i.destH);
  io.scalar(i.opacity);
  io.scalar(i.contentW);
  io.scalar(i.contentH);
  io.scalar(i.backdropSigma);
  io.scalar(i.backdropRadius);
  io.scalar(i.backdropU0);
  io.scalar(i.backdropV0);
  io.scalar(i.backdropU1);
  io.scalar(i.backdropV1);
  io.boolean(i.backdropFlat);
  io.boolean(i.hasShadow);
  io.scalar(i.shadow.sigma);
  vec2(io, i.shadow.offset);
  io.color(i.shadow.color);
  io.scalar(i.shadow.expandPx);
  io.scalar(i.contentSigma);
  io.boolean(i.hasGlass);
  io.boolean(i.glassContainer);
  lensFields(io, i.lens);
  io.scalar(i.glassU0);
  io.scalar(i.glassV0);
  io.scalar(i.glassU1);
  io.scalar(i.glassV1);
}

template <class Io, class F>
void frameFields(Io& io, F& f) {
  vec2(io, f.logicalSize);
  rect(io, f.dirtyRect);
}

template <class T, class Fields>
constexpr std::uint32_t sizeOf(Fields fields) {
  Count count;
  T value{};
  fields(count, value);
  return count.size();
}

constexpr auto kQuad = [](auto& io, auto& v) { quadFields(io, v); };
constexpr auto kBlit = [](auto& io, auto& v) { blitFields(io, v); };
constexpr auto kGradient = [](auto& io, auto& v) { gradientFields(io, v); };
constexpr auto kBackdropPill = [](auto& io, auto& v) { backdropPillFields(io, v); };
constexpr auto kLensPill = [](auto& io, auto& v) { lensPillFields(io, v); };
constexpr auto kFrame = [](auto& io, auto& v) { frameFields(io, v); };
constexpr auto kIsolate = [](auto& io, auto& v) { isolateFields(io, v); };

// Wire stride equals the struct: the memcpy path depends on it.
static_assert(sizeOf<p::Quad>(kQuad) == sizeof(p::Quad) && sizeof(p::Quad) == 36);
static_assert(sizeOf<p::BlitQuad>(kBlit) == sizeof(p::BlitQuad) && sizeof(p::BlitQuad) == 56);
static_assert(
    sizeOf<p::GradientQuad>(kGradient) == sizeof(p::GradientQuad) && sizeof(p::GradientQuad) == 208
);
static_assert(
    sizeOf<p::BackdropPill>(kBackdropPill) == sizeof(p::BackdropPill) &&
    sizeof(p::BackdropPill) == 20
);
static_assert(sizeOf<p::LensPill>(kLensPill) == sizeof(p::LensPill) && sizeof(p::LensPill) == 28);
static_assert(offsetof(p::GradientQuad, p0x) == 24 && offsetof(p::GradientQuad, stopCount) == 45);
static_assert(offsetof(p::GradientQuad, offsets) == 48 && offsetof(p::BlitQuad, imageId) == 48);
static_assert(offsetof(p::LensPill, kind) == 24 && offsetof(p::Quad, plus) == 32);

template <class T>
constexpr bool kMemcpy =
    std::endian::native == std::endian::little && std::is_trivially_copyable_v<T>;

template <class T, class Fields>
void writeArray(w::Writer& writer, std::uint16_t type, const std::vector<T>& items, Fields fields) {
  if (items.empty()) {
    return;
  }
  constexpr std::uint32_t stride = sizeOf<T>(Fields{});
  auto builder = writer.begin(type, kArrayFixed, w::kMustUnderstand);
  builder.set(kStrideOffset, stride);
  const auto tail =
      builder.allocTail(kArrayTail, static_cast<std::uint32_t>(stride * items.size()));
  if constexpr (kMemcpy<T>) {
    std::memcpy(tail.data(), items.data(), tail.size());
  }
  for (std::size_t i = 0; i < items.size(); ++i) {
    if constexpr (kMemcpy<T>) {
      ZeroPads zero(tail.data() + i * stride);
      fields(zero, items[i]);
    } else {
      Put put(tail.data() + i * stride);
      fields(put, items[i]);
    }
  }
}

// Each source byte is read once into `into`; values are checked on that copy.
template <class T, class Fields>
bool appendArray(std::vector<T>& into, w::RecordView record, Fields fields) {
  const auto stride = record.field<std::uint32_t>(kStrideOffset);
  const auto tail = record.tail(kArrayTail);
  if (stride == 0 || !tail || tail->size() % stride != 0) {
    return false;
  }
  const std::size_t first = into.size();
  const std::size_t count = tail->size() / stride;
  into.resize(first + count);
  bool ok = true;
  if (kMemcpy<T> && stride == sizeof(T)) {
    std::memcpy(static_cast<void*>(into.data() + first), tail->data(), tail->size());
  } else {
    for (std::size_t i = 0; i < count; ++i) {
      Get get(tail->data() + i * stride, stride);
      fields(get, into[first + i]);
      ok = ok && get.ok();
    }
  }
  Check check;
  for (std::size_t i = first; i < into.size(); ++i) {
    fields(check, into[i]);
  }
  return ok && check.ok();
}

template <class T, class Fields>
std::optional<T> readFixed(w::RecordView record, Fields fields) {
  T value{};
  Get get(record.bytes().data() + w::kHeaderSize, record.fixedSize() - w::kHeaderSize);
  fields(get, value);
  Check check;
  fields(check, value);
  return get.ok() && check.ok() ? std::optional<T>(std::move(value)) : std::nullopt;
}

template <class T, class Fields>
void writeFixed(w::Writer& writer, std::uint16_t type, const T& value, Fields fields) {
  Count count;
  fields(count, value);
  Put put(writer.begin(type, w::kHeaderSize + count.size(), w::kMustUnderstand).fields().data());
  fields(put, value);
}

bool validContent(const p::Isolate& i) {
  return i.contentW >= 1 && i.contentH >= 1 && i.contentW <= kMaxContentSide &&
         i.contentH <= kMaxContentSide;
}

constexpr w::TailRule kArrayTails[] = {{kArrayTail, 1}};

// Framing only; values are checked while decoding, in the same pass.
constexpr w::TypeRule kRules[] = {
    {kFrameRecord, {}},
    {kQuadsRecord, kArrayTails},
    {kBlitsRecord, kArrayTails},
    {kGradientsRecord, kArrayTails},
    {kIsolateBeginRecord, {}},
    {kIsolateEndRecord, {}},
    {kBackdropPillsRecord, kArrayTails},
    {kLensPillsRecord, kArrayTails},
};

template <class Draws>
void writeDraws(w::Writer& writer, const Draws& d);

void writeIsolate(w::Writer& writer, const p::Isolate& iso) {
  writeFixed(writer, kIsolateBeginRecord, iso, kIsolate);
  writeArray(writer, kBackdropPillsRecord, iso.backdropPills, kBackdropPill);
  writeArray(writer, kLensPillsRecord, iso.lensPills, kLensPill);
  writeDraws(writer, iso);
  writer.begin(kIsolateEndRecord, w::kHeaderSize, w::kMustUnderstand);
}

template <class Draws>
void writeDraws(w::Writer& writer, const Draws& d) {
  writeArray(writer, kQuadsRecord, d.quads, kQuad);
  writeArray(writer, kBlitsRecord, d.blits, kBlit);
  writeArray(writer, kGradientsRecord, d.gradients, kGradient);
  for (const p::Isolate& child : d.isolates) {
    writeIsolate(writer, child);
  }
}

}  // namespace

void writeFrame(const p::FramePacket& packet, std::vector<std::byte>& out) {
  w::Writer writer(out);
  writeFixed(writer, kFrameRecord, packet, kFrame);
  writeDraws(writer, packet);
}

bool readFrame(std::span<const std::byte> message, p::ImageStore images, p::FramePacket& out) {
  p::Isolate root;
  root.quads = std::move(out.quads);
  root.blits = std::move(out.blits);
  root.gradients = std::move(out.gradients);
  root.quads.clear();
  root.blits.clear();
  root.gradients.clear();
  out.isolates.clear();
  bool ok = w::validate(message, kRules) == w::Status::Ok;
  bool sawFrame = false;
  std::vector<p::Isolate*> open;
  if (ok) {
    w::forEachKnown(message, kRules, [&](w::RecordView record) {
      if (!ok) {
        return;
      }
      const auto type = record.type();
      if (type == kFrameRecord) {
        const auto frame = readFixed<p::FramePacket>(record, kFrame);
        ok = !sawFrame && frame;
        if (ok) {
          out.logicalSize = frame->logicalSize;
          out.dirtyRect = frame->dirtyRect;
        }
        sawFrame = true;
        return;
      }
      if (!sawFrame) {
        ok = false;
        return;
      }
      p::Isolate& at = open.empty() ? root : *open.back();
      switch (type) {
        case kQuadsRecord:
          ok = appendArray(at.quads, record, kQuad);
          break;
        case kBlitsRecord:
          ok = appendArray(at.blits, record, kBlit);
          break;
        case kGradientsRecord:
          ok = appendArray(at.gradients, record, kGradient);
          break;
        case kBackdropPillsRecord:
          ok = !open.empty() && appendArray(at.backdropPills, record, kBackdropPill);
          break;
        case kLensPillsRecord:
          ok = !open.empty() && appendArray(at.lensPills, record, kLensPill) &&
               at.lensPills.size() <= static_cast<std::size_t>(p::kMaxLensPills);
          break;
        case kIsolateBeginRecord: {
          auto iso = readFixed<p::Isolate>(record, kIsolate);
          ok =
              iso && validContent(*iso) && open.size() < static_cast<std::size_t>(kMaxIsolateDepth);
          if (ok) {
            at.isolates.push_back(std::move(*iso));
            open.push_back(&at.isolates.back());
          }
          break;
        }
        case kIsolateEndRecord:
          ok = !open.empty();
          if (ok) {
            open.pop_back();
          }
          break;
        default:
          ok = false;
      }
    });
  }
  ok = ok && sawFrame && open.empty();
  if (!ok) {
    root.quads.clear();
    root.blits.clear();
    root.gradients.clear();
    root.isolates.clear();
  }
  out.quads = std::move(root.quads);
  out.blits = std::move(root.blits);
  out.gradients = std::move(root.gradients);
  out.isolates = std::move(root.isolates);
  out.images = std::move(images);
  return ok;
}

std::optional<p::FramePacket> readFrame(std::span<const std::byte> message, p::ImageStore images) {
  p::FramePacket packet;
  if (!readFrame(message, std::move(images), packet)) {
    return std::nullopt;
  }
  return packet;
}

}  // namespace glim::guest
