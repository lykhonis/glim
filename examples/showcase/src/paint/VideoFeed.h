#pragma once

#include <glim/paint/Context.h>

#include <cstdint>

#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {
namespace paint {

// Checker-video texture, embedder-owned. T4 is software-only: pixels go
// through addImage; the GPU overload (wrapNativeTexture via Device)
// lands with the windowed run, which links glim::glim. Either way the
// id (not pixels) is what crosses the UI/compositor boundary.
struct VideoFeed {
  std::uint32_t id = 0;
  bool foreign = false;

  std::uint32_t ensure(::glim::paint::Context& ctx) {
    if (id != 0) return id;
    constexpr int k = 64;
    static std::uint8_t px[k * k * 4];
    static bool baked = false;
    if (!baked) {
      for (int y = 0; y < k; ++y)
        for (int x = 0; x < k; ++x) {
          bool cell = ((x / 8) + (y / 8)) % 2 == 0;
          std::size_t i = static_cast<std::size_t>((y * k + x) * 4);
          px[i + 0] = cell ? 0x2a : 0xf0;
          px[i + 1] = cell ? 0x9d : 0xd5;
          px[i + 2] = cell ? 0x8f : 0xa8;
          px[i + 3] = 0xff;
        }
      baked = true;
    }
    id = ctx.addImage(k, k, px);
    foreign = false;
    return id;
  }
};

// Paint sink concept (duck-typed, zero cost). Every method takes plain
// data — rects, colors, strings, ids. Two sinks ship in T4:
// ContextSink (local Scene) and OpsRecorder (inspectable UI recording +
// ArchiveSink source for the future network path). A new sink
// implements these eleven methods; the walk never names a sink type.
template <class T, class S, class = void>
struct has_paint_with : std::false_type {};
template <class T, class S>
struct has_paint_with<
    T,
    S,
    std::void_t<decltype(std::declval<const T&>().paint(
        std::declval<const Box&>(),
        std::declval<S&>(),
        std::declval<const Engine&>()
    ))>> : std::true_type {};

inline Rect toRect(const Box& b) {
  return Rect{{b.x, b.y}, {b.w, b.h}};
}

struct ContextSink {
  ::glim::paint::Context& ctx;
  explicit ContextSink(::glim::paint::Context& c) : ctx(c) {}
  ::glim::paint::Context& context() { return ctx; }
  // The ONLY place that knows entry kinds. New elements add one branch
  // here; OpsRecorder and the archive never change. Unknown kinds are
  // ignored (old painters skip new elements instead of breaking).
  void entry(const Rec::Entry& en, VideoFeed& feed) {
    const std::string& k = en.kind;
    if (k == "Button" || k == "Button.primary") {
      bool primary = (k == "Button.primary");
      fillRounded(
          en.box,
          Tokens::kButtonRadius,
          primary ? Tokens::kButtonPrimaryFill : Tokens::kButtonFill
      );
      text(
          en.box.x + static_cast<float>(Tokens::kButtonPadH),
          en.box.y + (en.box.h - Tokens::kTypeLine) * 0.5f,
          en.text,
          primary ? Tokens::kInkOnDark : Tokens::kInkOnLight
      );
    } else if (k == "Card") {
      fillRounded(en.box, en.radius, Tokens::kCardFill);
    } else if (k == "Lines") {
      text(en.box.x, en.box.y, en.text, Tokens::kInkOnLight);
    } else if (k == "Veil") {
      fill(en.box, Tokens::kScrim);
    } else if (k == "Video") {
      blitVideo(en.box, feed);
    }
  }
  void blitVideo(const Box& b, VideoFeed& feed) {
    std::uint32_t vid = feed.ensure(ctx);
    if (vid != 0) blit(b, vid, feed.foreign);
  }

 private:
  void fill(const Box& b, std::uint32_t rgba) {
    ctx.setFillColor(rgba);
    ctx.fill(toRect(b));
  }
  void fillRounded(const Box& b, int radius, std::uint32_t rgba) {
    ctx.setFillColor(rgba);
    ctx.fillRounded(toRect(b), Radius{static_cast<float>(radius)});
  }
  void text(
      float x, float y, const std::string& s, std::uint32_t rgba
  ) {
    ctx.setFillColor(rgba);
    ctx.text({x, y}, s.c_str(), Tokens::kTypeLine);
  }
  void blit(const Box& b, std::uint32_t imageId, bool foreign) {
    ctx.blit(
        toRect(b),
        foreign ? ::glim::paint::Matter::foreign(imageId)
                : ::glim::paint::Matter::sampled(imageId)
    );
  }

 public:
  void pushGlass(const Box& b, int radius, bool clear) {
    ::glim::paint::GroupParams p;
    p.bounds = toRect(b);
    p.clip = toRect(b);
    p.clipRadius = Radius{static_cast<float>(radius)};
    ::glim::paint::Glass g =
        clear ? ::glim::paint::Glass::clear() : ::glim::paint::Glass{};
    g.thicknessPx = Tokens::kGlassThickness;
    p.glass = g;
    ctx.pushGroup(p);
  }
  void pushClip(const Box& b) {
    ::glim::paint::GroupParams p;
    p.clip = toRect(b);
    ctx.pushGroup(p);
  }
  void pop() { ctx.popGroup(); }
  void slot(const Box& b, std::uint32_t id) { ctx.slot(toRect(b), id); }
  void stroke(
      const Box& b, int radius, float width, std::uint32_t rgba
  ) {
    ctx.setFillColor(rgba);
    ctx.strokeRect(
        toRect(b), Radius{static_cast<float>(radius)}, width
    );
  }
};

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
