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

// Paint sink concept (duck-typed, zero cost). Sinks interpret one
// thing: Rec entries plus structural grouping. `entry` carries opaque
// content (kind strings ride along); `pushGlass`/`pushClip`/`pop`,
// `slot`, `stroke` carry structure. ContextSink interprets entry
// kinds to pixels; OpsRecorder appends the serializable ops. New
// elements serialize with zero codec edits. Custom paint() is written
// once against AnySink (core) and runs on every sink.

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
      if (primary) {
        ::glim::paint::GradientStop stops[2] = {
            {0.0f, ::glim::Color{Tokens::kButtonPrimaryTop}},
            {1.0f, ::glim::Color{Tokens::kButtonPrimaryBottom}},
        };
        ctx.setFill(::glim::paint::Matter::linearGradient(
            {en.box.x, en.box.y},
            {en.box.x, en.box.y + en.box.h},
            stops,
            2
        ));
        ctx.fillRounded(
            toRect(en.box),
            Radius{static_cast<float>(Tokens::kButtonRadius)}
        );
      } else {
        fillRounded(en.box, Tokens::kButtonRadius, Tokens::kButtonFill);
      }
      text(
          en.box.x + static_cast<float>(Tokens::kButtonPadH),
          en.box.y + (en.box.h - Tokens::kTypeLine) * 0.5f,
          en.text,
          Tokens::kInkOnLight
      );
    } else if (k == "Card") {
      fillRounded(en.box, en.radius, Tokens::kCardFill);
      stroke(en.box, en.radius, 1.0f, Tokens::kCardStroke);
    } else if (k == "Lines") {
      text(en.box.x, en.box.y, en.text, Tokens::kInkOnLight);
    } else if (k == "Lines.dim") {
      text(en.box.x, en.box.y, en.text, Tokens::kInkSecondary);
    } else if (k == "Video") {
      blitVideo(en.box, feed);
    } else if (k == "Switch") {
      fillRounded(en.box, en.radius, Tokens::kCardFill);
    } else if (k == "Switch.on") {
      fillRounded(en.box, en.radius, Tokens::kAccent);
    } else if (k == "Switch.thumb") {
      fillRounded(en.box, en.radius, Tokens::kInkOnLight);
    } else if (k == "Check") {
      fillRounded(en.box, en.radius, Tokens::kCardFill);
      Box inner{
          en.box.x + 3.0f,
          en.box.y + 3.0f,
          en.box.w - 6.0f,
          en.box.h - 6.0f
      };
      fillRounded(inner, en.radius - 3, Tokens::kWindowBg);
    } else if (k == "Check.on") {
      fillRounded(en.box, en.radius, Tokens::kAccent);
    } else if (k == "Seg") {
      fillRounded(en.box, en.radius, Tokens::kCardFill);
    } else if (k == "Seg.active") {
      fillRounded(en.box, en.radius, Tokens::kButtonFill);
    } else if (k == "Slide") {
      fillRounded(en.box, en.radius, Tokens::kCardFill);
    } else if (k == "Slide.fill") {
      if (en.box.w > 0.5f)
        fillRounded(en.box, en.radius, Tokens::kAccent);
    } else if (k == "Slide.thumb") {
      fillRounded(en.box, en.radius, Tokens::kInkOnLight);
    } else if (k == "Dot") {
      fillRounded(en.box, en.radius, Tokens::kInkOnLight);
    } else if (k == "Dot.bg") {
      fillRounded(en.box, en.radius, Tokens::kCardFill);
    } else if (k == "Ring") {
      stroke(en.box, en.radius, 2.0f, Tokens::kInkOnLight);
    } else if (k == "Bar") {
      fillRounded(en.box, en.radius, Tokens::kInkOnLight);
    } else if (k == "Bar.bg") {
      fillRounded(en.box, en.radius, Tokens::kCardFill);
    } else if (k == "Hairline") {
      fill(en.box, Tokens::kHairline);
    } else if (k == "Display") {
      text(
          en.box.x,
          en.box.y + (en.box.h - Tokens::kTypeDisplay) * 0.5f,
          en.text,
          Tokens::kInkOnLight,
          Tokens::kTypeDisplay
      );
    } else if (k == "Tile") {
      fillRounded(en.box, en.radius, Tokens::kButtonFill);
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
      float x,
      float y,
      const std::string& s,
      std::uint32_t rgba,
      float size = Tokens::kTypeLine
  ) {
    // y is the TOP of the line box; Context::text takes a BASELINE
    // origin (Overlay::drawText adds ascent for the same reason).
    // Without this every label sits one ascent too high.
    ::glim::paint::TextSize m = ctx.measureText(s.c_str(), size);
    ctx.setFillColor(rgba);
    ctx.text({x, y + m.ascent}, s.c_str(), size);
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
