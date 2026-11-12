#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Renderer.h>
#include <glim/paint/Scene.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "GroupOrder.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  using glim::Rect;
  using glim::paint::GroupParams;
  using glim::paint::Lens;
  using glim::paint::LensVariant;

  {
    glim::paint::Context ctx;
    ctx.setSize({100, 100});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(Rect::fromSize({100, 100}));
    GroupParams p;
    p.lens = Lens{};
    p.bounds = Rect{{10, 10}, {40, 20}};
    p.clipRadius = glim::Radius{10.f};
    ctx.pushGroup(p);
    ctx.popGroup();
    ctx.finish();
    glim::paint::Stats stats{};
    glim::paint::Group merged = glim::paint::merge(std::move(ctx.scene().root), &stats);
    expect(merged.children.size() == 1, "glass group stays isolated");
    expect(merged.children.size() == 1 && glim::paint::hasGlass(*merged.children[0]), "hasGlass");
    expect(
        merged.children.size() == 1 && glim::paint::hasGlassWork(*merged.children[0]),
        "hasGlassWork"
    );
    expect(
        merged.children.size() == 1 && !glim::paint::hasBackdrop(*merged.children[0]),
        "glass is not backdrop"
    );
    expect(
        merged.children.size() == 1 && glim::paint::needsIsolate(*merged.children[0]),
        "glass isolates"
    );
    expect(
        merged.children.size() == 1 && !glim::paint::canMerge(*merged.children[0]),
        "canMerge refuses glass"
    );
  }

  {
    glim::paint::Context ctx;
    ctx.setSize({80, 40});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(Rect::fromSize({80, 40}));
    GroupParams p;
    p.lens = Lens{};
    p.bounds = Rect{{8, 4}, {32, 16}};
    p.clipRadius = glim::Radius{8.f};
    ctx.pushGroup(p);
    ctx.popGroup();
    ctx.finish();
    const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
    expect(pkt.isolates.size() >= 1, "glass encode isolate");
    bool found = false;
    for (const auto& iso : pkt.isolates) {
      if (!iso.hasGlass) {
        continue;
      }
      found = true;
      expect(iso.backdropSigma == 0.f, "independence: backdropSigma 0");
      expect(!iso.lensPills.empty(), "glass isolate carries pills");
      expect(iso.glassU1 > iso.glassU0 && iso.glassV1 > iso.glassV0, "glass dest UV");
      expect(iso.contentW > 32, "guttered glassSurface wider than bounds");
      expect(iso.contentH > 16, "guttered glassSurface taller than bounds");
    }
    expect(found, "found glass isolate");
    expect(pkt.stats.glassPassCount >= 1, "encode glassPassCount");
    expect(pkt.stats.backdropCount == 0, "independence: no backdrop pyramid");
  }

  {
    glim::paint::GroupOrder runs;
    expect(!runs.next(false, true), "first glass run is not a restart");
    expect(!runs.next(false, false), "overlay after glass");
    expect(runs.next(false, true), "glass after an overlay starts a new run");
    expect(!runs.next(false, true), "consecutive glass stays in the run");
  }
  {
    glim::paint::Context ctx;
    ctx.setSize({80, 40});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(Rect::fromSize({80, 40}));
    GroupParams first;
    first.lens = Lens{};
    first.bounds = Rect{{4, 4}, {20, 12}};
    ctx.pushGroup(first);
    ctx.popGroup();
    GroupParams overlay;
    overlay.opacity = 0.5f;
    ctx.pushGroup(overlay);
    ctx.setFillColor(0xff00ff00);
    ctx.fill(Rect{{30, 4}, {10, 30}});
    ctx.popGroup();
    GroupParams second;
    second.lens = Lens{};
    second.bounds = Rect{{50, 4}, {20, 12}};
    ctx.pushGroup(second);
    ctx.popGroup();
    ctx.finish();
    const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
    expect(pkt.stats.glassPassCount == 2, "glass, overlay, glass: both glass runs encode");
    std::vector<std::uint8_t> buf(80 * 40 * 4);
    glim::paint::Renderer renderer(buf.data(), 80, 40);
    renderer.draw(ctx.scene());
    expect(buf[(20 * 80 + 60) * 4 + 3] != 0, "glass, overlay, glass: second glass renders");
  }

  {
    glim::paint::Context ctx;
    ctx.setSize({80, 40});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(Rect::fromSize({80, 40}));
    GroupParams frost;
    frost.backdropBlur = 16.f;
    frost.bounds = Rect{{4, 4}, {20, 12}};
    ctx.pushGroup(frost);
    ctx.popGroup();
    GroupParams g;
    g.lens = Lens{};
    g.bounds = Rect{{40, 4}, {20, 12}};
    ctx.pushGroup(g);
    ctx.popGroup();
    ctx.finish();
    const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
    expect(pkt.stats.backdropCount == 1, "both: backdropCount");
    expect(pkt.stats.glassPassCount == 1, "both: glassPassCount");
    int nIso = 0;
    for (const auto& iso : pkt.isolates) {
      if (iso.hasGlass || iso.backdropSigma > 0.f) {
        ++nIso;
      }
    }
    expect(nIso == 2, "both: two effect isolates");
  }

  {
    GroupParams empty;
    empty.glassContainer = true;
    empty.bounds = Rect{{0, 0}, {40, 20}};
    glim::paint::Group g;
    g.params = empty;
    expect(!glim::paint::hasGlassWork(g), "empty container has no work");
    expect(!glim::paint::needsIsolate(g), "empty container does not isolate");
  }

  {
    glim::paint::Context ctx;
    ctx.setSize({100, 40});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(Rect::fromSize({100, 40}));
    GroupParams box;
    box.glassContainer = true;
    box.bounds = Rect{{4, 4}, {90, 24}};
    ctx.pushGroup(box);
    GroupParams a;
    a.lens = Lens{};
    a.bounds = Rect{{8, 6}, {24, 16}};
    a.clipRadius = glim::Radius{8.f};
    ctx.pushGroup(a);
    ctx.popGroup();
    GroupParams b;
    b.lens = Lens{};
    b.bounds = Rect{{40, 6}, {24, 16}};
    b.clipRadius = glim::Radius{8.f};
    ctx.pushGroup(b);
    ctx.popGroup();
    ctx.popGroup();
    ctx.finish();
    const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
    expect(pkt.stats.glassPassCount == 1, "container: one glass pass");
    bool found = false;
    for (const auto& iso : pkt.isolates) {
      if (!iso.hasGlass) {
        continue;
      }
      found = true;
      expect(iso.lensPills.size() == 2, "bound-only children emit two pills");
      expect(iso.isolates.empty() || !iso.isolates[0].hasGlass, "inner glass flags cleared");
    }
    expect(found, "container produced a glass isolate");
    glim::paint::LensPill pills[glim::paint::kMaxLensPills];
    glim::paint::Group merged = glim::paint::merge(ctx.scene().root, nullptr);
    expect(merged.children.size() == 1, "container stays a child");
    const int n = glim::paint::collectLensPills(*merged.children[0], pills);
    expect(n == 2, "collectLensPills bound-only fallback");
  }

  {
    glim::paint::Context ctx;
    ctx.setSize({80, 40});
    ctx.beginFrame();
    ctx.setFillColor(0x00ff00ff);
    ctx.fill(Rect::fromSize({80, 40}));
    GroupParams g;
    g.lens = Lens{};
    g.bounds = Rect{{8, 4}, {32, 16}};
    ctx.pushGroup(g);
    ctx.popGroup();
    ctx.setFillColor(0xffffffff);
    ctx.fill(Rect{{50, 4}, {10, 10}});
    ctx.finish();
    const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
    expect(pkt.stats.glassPassCount == 1, "merge lock: still one glass pass");
    expect(pkt.isolates.size() >= 2, "opaque sibling after glass is not folded ahead");
  }

  {
    glim::paint::Context ctx;
    ctx.setSize({80, 40});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(Rect::fromSize({80, 40}));
    GroupParams outer;
    outer.lens = Lens{};
    outer.bounds = Rect{{8, 4}, {48, 24}};
    ctx.pushGroup(outer);
    GroupParams inner;
    inner.lens = Lens{};
    inner.bounds = Rect{{12, 8}, {16, 12}};
    ctx.pushGroup(inner);
    ctx.popGroup();
    ctx.popGroup();
    ctx.finish();
    const glim::paint::FramePacket pkt = glim::paint::encode(ctx.scene(), 1.f);
    expect(pkt.stats.glassPassCount == 1, "nested glass skipped inner snapshot");
  }

  {
    GroupParams p;
    p.lens = Lens{};
    p.lens->variant = LensVariant::Identity;
    glim::paint::Group g;
    g.params = p;
    expect(glim::paint::hasGlass(g), "Identity is glass");
    expect(glim::paint::needsIsolate(g), "Identity isolates");
    expect(!glim::paint::hasBackdrop(g), "Identity is not backdrop");
  }

  {
    const glim::paint::Lens metal = glim::paint::Lens::liquidMetal(glim::Color{0xd8a84eff});
    expect(metal.material == glim::paint::LensMaterial::LiquidMetal, "metal preset material");
    expect(metal.transmit == 0.f, "metal preset opaque");
    expect(glim::paint::lensBlurRadius(metal) == 0.f, "metal skips the backdrop blur pass");
    const glim::paint::Lens water = glim::paint::Lens::water();
    expect(water.material == glim::paint::LensMaterial::Water, "water preset material");
    expect(water.transmit == 1.f, "water preset transmissive");
    expect(
        glim::paint::lensBlurRadius(water) > 0.f && glim::paint::lensBlurRadius(water) < 20.f,
        "water blur small and sharp"
    );
  }

  {
    // Time is read by the water ripple field and moving swells.
    using glim::paint::LensMotion;
    using glim::paint::LensMotionCurve;
    using glim::paint::lensSwellPhase;
    const glim::paint::Lens glass{};
    expect(!glim::paint::lensAnimates(glass), "plain glass ignores time");
    const glim::paint::Lens metal = glim::paint::Lens::liquidMetal(glim::Color{0xd8a84eff});
    expect(glim::paint::lensAnimates(metal), "flowing metal animates");
    expect(!glim::paint::lensRipples(metal), "metal has no ripple time");
    glim::paint::Lens stillMetal = metal;
    stillMetal.motion.period = 0.f;
    expect(!glim::paint::lensAnimates(stillMetal), "still metal ignores time");
    const glim::paint::Lens water = glim::paint::Lens::water();
    expect(glim::paint::lensAnimates(water), "wavy water animates");
    glim::paint::Lens chop = glim::paint::Lens::water();
    chop.rippleAmp = 1.5f;
    expect(glim::paint::lensRipples(chop), "rippling water reads time");
    glim::paint::Lens still = glim::paint::Lens::water();
    still.motion.period = 0.f;
    expect(!glim::paint::lensAnimates(still), "still water ignores time");
    glim::paint::Lens flatWater = glim::paint::Lens::water();
    flatWater.swellAmp = 0.f;
    expect(!glim::paint::lensAnimates(flatWater), "flat water ignores time");
    glim::paint::Lens flat = metal;
    flat.flatten = true;
    expect(!glim::paint::lensAnimates(flat), "flattened metal ignores time");

    // Surge, rest, surge back: phase over time for both curves.
    const auto phaseAt = [](glim::paint::Lens g, double t) {
      g.timeSec = static_cast<float>(t);
      return lensSwellPhase(g);
    };
    for (const LensMotionCurve curve : {LensMotionCurve::Smooth, LensMotionCurve::Spring}) {
      glim::paint::Lens g = metal;
      g.motion = LensMotion{4.f, 0.6f, 0.5f, curve, 0.2f};
      expect(phaseAt(g, 0.0) == 0.f, "surge starts at rest");
      expect(phaseAt(g, 0.02) < 0.01f, "surge starts gently");
      expect(
          phaseAt(g, 2.0) == phaseAt(g, 2.7) && phaseAt(g, 2.7) == phaseAt(g, 3.99),
          "surface rests between surges"
      );
      expect(std::fabs(phaseAt(g, 3.0) - 0.6f) < 1e-5f, "a surge travels its distance");
      expect(
          phaseAt(g, 6.5) == phaseAt(g, 7.9) && std::fabs(phaseAt(g, 7.f)) < 1e-5f,
          "the next surge slides back and rests"
      );
      for (double t = 0.0; t < 8.0; t += 0.37) {
        expect(
            std::fabs(phaseAt(g, t) - phaseAt(g, t + 8.0)) < 1e-5f, "motion loops every 2 periods"
        );
      }
      // timeSec is a float: at ~4e5 s it resolves 1/32 s, so sample there.
      for (double t = 0.0; t < 8.0; t += 0.25) {
        expect(
            std::fabs(phaseAt(g, t) - phaseAt(g, t + 8.0 * 50000.0)) < 1e-4f,
            "motion loops after long uptime"
        );
      }
      float peak = 0.f;
      float worstStep = 0.f;
      for (double t = 0.0; t < 8.0; t += 0.01) {
        peak = std::max(peak, phaseAt(g, t));
        worstStep = std::max(worstStep, std::fabs(phaseAt(g, t + 0.01) - phaseAt(g, t)));
      }
      expect(worstStep < 0.02f, "motion has no jumps");
      if (curve == LensMotionCurve::Spring) {
        expect(std::fabs(peak - 0.6f * 1.2f) < 0.02f, "spring overshoots by bounce");
      } else {
        expect(peak <= 0.6f + 1e-5f, "smooth never overshoots");
      }
    }
    // Flow never rests: constant rate, wrapped into [0, 2*pi).
    {
      glim::paint::Lens g = metal;
      g.motion = LensMotion{4.f, 0.6f, 1.f, LensMotionCurve::Flow, 0.2f};
      expect(std::fabs(phaseAt(g, 2.0) - 0.3f) < 1e-5f, "flow drifts at travel per period");
      for (double t = 0.0; t < 40.0; t += 0.25) {
        const float step = phaseAt(g, t + 0.25) - phaseAt(g, t);
        const bool wrapped = step < 0.f;
        expect(wrapped || std::fabs(step - 0.0375f) < 1e-4f, "flow is steady, never rests");
      }
      const double loop = 2.0 * 3.141592653589793 * 4.0 / 0.6;
      expect(
          std::fabs(std::sin(phaseAt(g, 5.0)) - std::sin(phaseAt(g, 5.0 + loop))) < 1e-4f,
          "flow loops every 2*pi*period/travel"
      );
    }
    stillMetal.timeSec = 3.f;
    expect(lensSwellPhase(stillMetal) == 0.f, "still metal has no phase");
  }

  {
    // Thickness follows plate size like the isolate gutter.
    const glim::paint::Lens g{};
    const float tiny = glim::paint::lensThicknessFor(g, glim::Rect{{0, 0}, {10, 10}});
    const float ref = glim::paint::lensThicknessFor(g, glim::Rect{{0, 0}, {200, 80}});
    const float huge = glim::paint::lensThicknessFor(g, glim::Rect{{0, 0}, {2000, 800}});
    const float empty = glim::paint::lensThicknessFor(g, glim::Rect{{0, 0}, {0, 0}});
    expect(empty == g.thicknessPx, "empty plate keeps base thickness");
    expect(tiny >= g.thicknessPx && tiny < ref, "small plate barely scales");
    expect(ref == g.thicknessPx * 1.6f, "reference area hits full scale");
    expect(huge == ref, "scale caps at 1.6x");
  }

  {
    const int w = 64;
    const int h = 48;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(w * h * 4), 0);
    glim::paint::Renderer renderer(buf.data(), w, h);
    glim::paint::Context ctx;
    ctx.setSize({static_cast<float>(w), static_cast<float>(h)});
    ctx.beginFrame();
    ctx.setFillColor(0xff0000ff);
    ctx.fill(Rect::fromSize({static_cast<float>(w), static_cast<float>(h)}));
    GroupParams p;
    p.lens = Lens{};
    p.bounds = Rect{{16.f, 12.f}, {32.f, 20.f}};
    p.clipRadius = glim::Radius{10.f};
    ctx.pushGroup(p);
    ctx.popGroup();
    ctx.finish();
    renderer.draw(ctx.scene());
    const std::size_t center = static_cast<std::size_t>((22 * w + 32) * 4);
    expect(buf[center] > 80, "cpu glass plate samples dest");
    expect(
        buf[center + 1] < 80 && buf[center + 2] < 80, "cpu glass plate is dest red, not warp black"
    );
  }

  {
    // Material overrides reach the CPU reference: presets carry the
    // defaults, and changing a knob changes the rendered plate.
    const Lens metal = Lens::liquidMetal();
    expect(
        metal.swellAmp == 0.8f && metal.swellScale.x == 0.55f && metal.swellScale.y == 1.1f,
        "liquidMetal swell defaults"
    );
    expect(
        metal.motion.period == 4.f && metal.motion.travel == 0.6f &&
            metal.motion.curve == glim::paint::LensMotionCurve::Flow && metal.envLight == 0.f,
        "liquidMetal slow continuous flow, studio only"
    );
    expect(
        Lens::water().swellAmp == 0.6f && Lens::water().motion.period == 3.5f &&
            Lens::water().motion.curve == glim::paint::LensMotionCurve::Smooth,
        "water swell defaults"
    );
    expect(
        Lens{}.swellAmp == 0.f && Lens{}.motion.period == 0.f && Lens{}.envLight == 0.f,
        "plain glass is flat, still, unlit"
    );
    const auto render = [](const Lens& g) {
      const int w = 160;
      const int h = 80;
      std::vector<std::uint8_t> buf(static_cast<std::size_t>(w * h * 4), 0);
      glim::paint::Renderer renderer(buf.data(), w, h);
      glim::paint::Context ctx;
      ctx.setSize({static_cast<float>(w), static_cast<float>(h)});
      ctx.beginFrame();
      for (int i = 0; i < 4; ++i) {
        ctx.setFillColor(i % 2 ? 0x2a9d8fff : 0xe9c46aff);
        ctx.fill(Rect{{static_cast<float>(i * 40), 0.f}, {40.f, static_cast<float>(h)}});
      }
      GroupParams p;
      p.lens = g;
      p.bounds = Rect{{10.f, 10.f}, {140.f, 60.f}};
      p.clip = p.bounds;
      p.clipRadius = glim::Radius{30.f};
      ctx.pushGroup(p);
      ctx.popGroup();
      ctx.finish();
      renderer.draw(ctx.scene());
      return buf;
    };
    const auto differs = [](const std::vector<std::uint8_t>& a,
                            const std::vector<std::uint8_t>& b) {
      std::size_t n = 0;
      for (std::size_t i = 0; i < a.size(); ++i) {
        n += (a[i] > b[i] ? a[i] - b[i] : b[i] - a[i]) > 8 ? 1 : 0;
      }
      return n > 200;
    };
    const auto base = render(metal);
    Lens flat = metal;
    flat.swellAmp = 0.f;
    expect(differs(base, render(flat)), "metal swellAmp override renders");
    Lens tight = metal;
    tight.swellScale = glim::Vec2{1.5f, 1.5f};
    expect(differs(base, render(tight)), "metal swellScale override renders");
    Lens lit = metal;
    lit.envLight = 0.6f;
    expect(differs(base, render(lit)), "metal envLight override renders");
    // No glow: the backdrop just outside the plate (x=5, mid height, 5px
    // left of the cap) is untouched, and the face never shows the backdrop:
    // the face pixel is grey (silver), not the yellow/teal stripe under it.
    const std::size_t outside = static_cast<std::size_t>((40 * 160 + 5) * 4);
    expect(
        base[outside] == 0xe9 && base[outside + 1] == 0xc4 && base[outside + 2] == 0x6a,
        "metal casts no glow on its surroundings"
    );
    // Flow: the face keeps drifting and loops seamlessly.
    const auto at = [](Lens g, float t) {
      g.timeSec = t;
      return g;
    };
    const float metalLoop = 6.2831853f * metal.motion.period / metal.motion.travel;
    const auto t3 = render(at(metal, 3.f));
    expect(differs(base, render(at(metal, 6.f))), "metal flows with time");
    expect(t3 != render(at(metal, 4.f)), "metal keeps flowing, never rests");
    expect(!differs(render(at(metal, metalLoop)), base), "metal flow loops seamlessly");
    Lens halted = at(metal, 2.f);
    halted.motion.period = 0.f;
    expect(render(halted) == base, "period 0 keeps metal still");
    const std::size_t face = static_cast<std::size_t>((40 * 160 + 60) * 4);
    const int spread = std::max({base[face], base[face + 1], base[face + 2]}) -
                       std::min({base[face], base[face + 1], base[face + 2]});
    expect(spread < 20, "metal face is grey, not the backdrop");
    const Lens water = Lens::water();
    Lens calm = water;
    calm.swellAmp = 0.f;
    expect(differs(render(water), render(calm)), "water swellAmp override renders");
    expect(
        differs(render(water), render(at(water, water.motion.period - 0.5f))),
        "water waves move with time"
    );
    expect(
        render(at(water, 2.f * water.motion.period)) == render(water),
        "water waves loop after 2 periods"
    );
  }

  if (failures) {
    std::cerr << failures << " glass tests failed\n";
    return EXIT_FAILURE;
  }

  // Uniform liveness: every optical GlassUniforms field must be read
  // (member access) by each lens shader. Catches dead uniforms.
  {
    std::string self = __FILE__;
    if (!self.empty() && self[0] != '/') {
      // Relative __FILE__: resolve against the process working directory.
      char cwd[4096] = {};
      if (::getcwd(cwd, sizeof(cwd)) != nullptr && cwd[0] != '\0') {
        self = std::string(cwd) + "/" + self;
      }
    }
    const std::string marker = "packages/glim-paint/tests/";
    std::string root;
    const std::size_t cut = self.rfind(marker);
    if (cut != std::string::npos) {
      root = self.substr(0, cut);
    }
    // The single GLSL source; Metal and WGSL are generated from it.
    const char* shaders[] = {"packages/glim-gpu/shaders/glass.frag"};
    // Previously-dead fields first, then the semantic switches.
    const char* fields[] = {
        "fresnelRange",  "fresnelHardness",  "fresnelIntensity", "glareAngle", "glareRange",
        "glareHardness", "glareConvergence", "glareIntensity",   "edge",       "tint",
        "blurEdge",      "dimmer",           "lumaOn",           "dispersion", "ior",
        "thickness",     "refDistance",      "material",         "transmit",   "rippleAmp",
        "timeSec",       "envSoft",          "absorbK",          "swellAmp",   "swellScale",
        "envLight",      "swellPhase",
    };
    for (const char* rel : shaders) {
      const std::string path = root + rel;
      std::ifstream in(path);
      if (!in) {
        std::cerr << "FAIL: cannot open shader " << path << '\n';
        ++failures;
        continue;
      }
      std::ostringstream ss;
      ss << in.rdbuf();
      const std::string src = ss.str();
      for (const char* f : fields) {
        // Member read: ".field" not followed by an identifier char is a use.
        // The declaration ("float edge;") has no dot, so it never matches.
        const std::string want = std::string(".") + f;
        bool live = false;
        for (std::size_t at = src.find(want); at != std::string::npos;
             at = src.find(want, at + 1)) {
          const std::size_t end = at + want.size();
          if (end >= src.size() ||
              !(std::isalnum(static_cast<unsigned char>(src[end])) || src[end] == '_')) {
            live = true;
            break;
          }
        }
        if (!live) {
          std::cerr << "FAIL: uniform ." << f << " unread in " << rel << '\n';
          ++failures;
        }
      }
    }
  }

  if (failures) {
    std::cerr << failures << " glass tests failed\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
