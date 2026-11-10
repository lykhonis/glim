#pragma once

#include <glim/math.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace glim::paint {

enum class Blend { SrcOver, Plus };

enum class MatterKind { Solid, Sampled, Foreign, Gradient };

enum class GradientKind { Linear, Radial };

struct GradientStop {
  float offset = 0.f;
  Color color{};
};

constexpr int kMaxGradientStops = 8;

enum class SampleFormat { Bgra8Unorm, Rgba8Unorm };

constexpr int kMaxImageSide = 4096;
constexpr std::uint64_t kMaxImageBytes = 16ull * 1024ull * 1024ull;

struct StoredImage {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;
  void* native = nullptr;
  SampleFormat format = SampleFormat::Rgba8Unorm;
  bool foreign = false;
};

// No gpu::Handle; shared across Context/Scene copies.
struct ImageStore {
  ImageStore();
  std::uint32_t add(int width, int height, const std::uint8_t* rgba);
  std::uint32_t wrap(void* native, int width, int height, SampleFormat format);
  void release(std::uint32_t id);
  std::optional<std::reference_wrapper<const StoredImage>> get(std::uint32_t id) const;

 private:
  struct Data {
    std::vector<StoredImage> slots{StoredImage{}};
  };
  std::shared_ptr<Data> data_;
};

enum class GlassVariant { Regular, Clear, Identity };

// Edge light is a per-side mask: Full is the jeweled plate, None a
// flat frost, Bottom alone a divider under a frosted bar.
enum class GlassEdge : std::uint8_t {
  None = 0,
  Left = 1,
  Right = 2,
  Top = 4,
  Bottom = 8,
  Full = 15,
};

constexpr bool hasEdge(GlassEdge e, GlassEdge side) {
  return (static_cast<std::uint8_t>(e) & static_cast<std::uint8_t>(side)) != 0;
}

enum class GlassMaterial : std::uint8_t { Glass, LiquidMetal, Water };

// How the swell moves. Smooth: eases in and out. Spring: a damped spring
// released toward the new rest, overshooting by `bounce` and settling.
// Flow: never rests; drifts at a constant `travel` per `period`.
enum class GlassMotionCurve : std::uint8_t { Smooth, Spring, Flow };

// Swell motion, driven by Glass::timeSec. Smooth/Spring: each period the
// surface surges by `travel`, settles within `moveShare` of the period, and
// rests for the remainder; surges alternate direction (a slosh), so the
// motion loops exactly every 2 * period. Flow: continuous drift that loops
// seamlessly every 2*pi * period / travel seconds (moveShare/bounce unused).
struct GlassMotion {
  float period = 0.f;      // seconds per surge; 0 = still
  float travel = 0.5f;     // swell phase per surge (radians); larger = farther
  float moveShare = 0.5f;  // share of the period spent moving, (0, 1]
  GlassMotionCurve curve = GlassMotionCurve::Smooth;
  float bounce = 0.2f;  // Spring overshoot as a share of travel, [0.01, 0.9]
};

struct Glass {
  GlassVariant variant = GlassVariant::Regular;
  GlassMaterial material = GlassMaterial::Glass;
  Color tint{};
  bool interactive = false;
  float ior = 1.45f;
  float thicknessPx = 24.f;
  float mergeKPx = 16.f;
  float refDistance = 0.35f;
  float dispersion = 0.12f;
  bool flatten = false;
  // Edge light per side; the frost, milk, and refraction are untouched.
  GlassEdge edge = GlassEdge::Full;
  // Material knobs. transmit 1 = refractive, 0 = fully reflective.
  // rippleAmp/timeSec drive the water normal field (0 = frozen).
  // envSoft is metal roughness: widens the blur of the mirrored surroundings.
  // absorbK scales water depth absorption.
  float transmit = 1.f;
  float rippleAmp = 0.f;
  float timeSec = 0.f;
  float envSoft = 0.35f;
  float absorbK = 1.f;
  // Surface swell: a plate-local wave field (metal folds, water waves).
  // swellAmp: strength (metal: fold tilt; water: share of the body bend
  // that waves the content beneath; 0 = flat). swellScale: wave frequency
  // per axis (> 1 = tighter waves; x < y stretches the waves along x).
  // motion: how the swell moves over timeSec (see GlassMotion; still by default).
  // envLight: how strongly the surroundings' brightness lights the metal's
  // mirrored studio (0 = studio only; the backdrop never shows through).
  float swellAmp = 0.f;
  Vec2 swellScale{1.f, 1.f};
  GlassMotion motion{};
  float envLight = 0.f;

  static Glass clear() {
    Glass g;
    g.variant = GlassVariant::Clear;
    g.thicknessPx = 16.f;
    g.ior = 1.33f;
    g.dispersion = 0.08f;
    return g;
  }

  // Opaque reflective metal; tint is the metal color (gold, chrome, gunmetal).
  // F0 goes metallic (0.9) in makeGlassUniforms via the material.
  // Natural (untinted) metal is silver; a tint colors it (gold, copper).
  static constexpr std::uint32_t kSilver = 0xc9ced4ff;
  static Glass liquidMetal(Color tint = Color{kSilver}) {
    Glass g;
    g.material = GlassMaterial::LiquidMetal;
    g.transmit = 0.f;
    g.tint = tint.a() != 0 ? tint : Color{kSilver};
    g.thicknessPx = 24.f;
    g.ior = 1.45f;
    g.envSoft = 0.85f;
    g.swellAmp = 0.8f;
    g.swellScale = Vec2{0.55f, 1.1f};
    // Molten flow: a slow, steady drift (loops every ~42 s).
    g.motion = GlassMotion{4.f, 0.6f, 1.f, GlassMotionCurve::Flow, 0.2f};
    return g;
  }

  // Shallow refractive water; tint is the absorption color (teal).
  // Still by default: the plate refracts the content beneath it.
  // Set rippleAmp > 0 for chop (then timeSec drives the normal field).
  static Glass water() {
    Glass g;
    g.material = GlassMaterial::Water;
    g.variant = GlassVariant::Clear;  // sharp core, no milk, no dimmer
    g.tint = Color{0x2a9d8fff};
    g.ior = 1.33f;
    g.dispersion = 0.1f;
    g.thicknessPx = 18.f;
    g.rippleAmp = 0.f;
    g.absorbK = 1.f;
    g.swellAmp = 0.6f;
    // A gentle swell rolling through, then calm.
    g.motion = GlassMotion{3.5f, 0.5f, 0.65f, GlassMotionCurve::Smooth, 0.2f};
    return g;
  }
};

// Gradient geometry shares the Shape rect space; factories sort stops by offset.
struct Matter {
  MatterKind kind = MatterKind::Solid;
  Color color{};
  std::uint32_t imageId = 0;
  Rect uv{{0.f, 0.f}, {1.f, 1.f}};
  GradientKind gradientKind = GradientKind::Linear;
  Vec2 gradientP0{};
  Vec2 gradientP1{};
  float gradientRadius = 0.f;
  int gradientStopCount = 0;
  GradientStop gradientStops[kMaxGradientStops]{};

  static Matter solid(Color color) {
    Matter m;
    m.kind = MatterKind::Solid;
    m.color = color;
    return m;
  }

  static Matter sampled(std::uint32_t imageId, Color tint = Color{255, 255, 255, 255}) {
    Matter m;
    m.kind = MatterKind::Sampled;
    m.color = tint;
    m.imageId = imageId;
    return m;
  }

  static Matter foreign(std::uint32_t imageId, Color tint = Color{255, 255, 255, 255}) {
    Matter m;
    m.kind = MatterKind::Foreign;
    m.color = tint;
    m.imageId = imageId;
    return m;
  }

  static Matter linearGradient(Vec2 begin, Vec2 end, const GradientStop* stops, int count) {
    Matter m;
    m.kind = MatterKind::Gradient;
    m.gradientKind = GradientKind::Linear;
    m.gradientP0 = begin;
    m.gradientP1 = end;
    m.setGradientStops(stops, count);
    return m;
  }

  static Matter radialGradient(Vec2 center, float radius, const GradientStop* stops, int count) {
    Matter m;
    m.kind = MatterKind::Gradient;
    m.gradientKind = GradientKind::Radial;
    m.gradientP0 = center;
    m.gradientRadius = radius > 0.f ? radius : 0.f;
    m.setGradientStops(stops, count);
    return m;
  }

  bool isGradient() const noexcept { return kind == MatterKind::Gradient; }

 private:
  void setGradientStops(const GradientStop* stops, int count);
};

inline void Matter::setGradientStops(const GradientStop* stops, int count) {
  gradientStopCount = 0;
  if (!stops || count <= 0) {
    return;
  }
  if (count > kMaxGradientStops) {
    count = kMaxGradientStops;
  }
  for (int i = 0; i < count; ++i) {
    gradientStops[i] = stops[i];
  }
  gradientStopCount = count;
  for (int i = 1; i < gradientStopCount; ++i) {
    GradientStop key = gradientStops[i];
    int j = i - 1;
    while (j >= 0 && gradientStops[j].offset > key.offset) {
      gradientStops[j + 1] = gradientStops[j];
      --j;
    }
    gradientStops[j + 1] = key;
  }
  // Legacy single-color paths degrade to the first stop, not black.
  color = gradientStops[0].color;
}

struct ShadowBlur {
  float sigma = 0.f;
  Vec2 offset{};
  Color color{0, 0, 0, 128};
  float expandPx = 0.f;
};

struct GroupParams {
  float opacity = 1.0f;
  Mat4 transform = Mat4::identity();
  Rect bounds{};
  bool isolate = false;
  Blend blend = Blend::SrcOver;
  Rect clip{};
  Radius clipRadius{};
  float backdropBlur = 0.f;
  bool backdropFlat = false;
  std::optional<Glass> glass;
  bool glassContainer = false;
  std::optional<ShadowBlur> shadow;
  float contentBlur = 0.f;
  std::uint32_t semantic = 0;
};

struct FillRect {
  Rect rect;
  Matter matter;
};

struct FillRounded {
  Rect rect;
  Radius radius;
  Matter matter;
};

struct FillChamfer {
  Rect rect;
  float cut = 0.f;
  Matter matter;
};

struct Stroke {
  Rect rect;
  Radius radius;
  float width = 1.f;
  Matter matter;
};

struct StrokeChamfer {
  Rect rect;
  float cut = 0.f;
  float width = 1.f;
  Matter matter;
};

struct Arc {
  Vec2 center{};
  float radius = 0.f;
  float thickness = 1.f;
  float startDeg = 0.f;
  float sweepDeg = 0.f;
  Matter matter;
};

struct Polyline {
  std::vector<Vec2> points;
  float width = 1.f;
  float dotPitch = 0.f;
  Matter matter;
};

struct Blit {
  Rect rect;
  Matter matter;
};

struct GlyphQuad {
  Rect dest;
  Rect uv;
};

struct GlyphRun {
  Vec2 origin{};
  float sizePx = 16.f;
  Color color{};
  std::uint32_t imageId = 0;
  std::vector<GlyphQuad> glyphs;
};

// Hosted native hole. Illegal under needsIsolate.
struct SlotHole {
  Rect rect{};
  std::uint32_t id = 0;
};

using Shape = std::variant<
    FillRect,
    FillRounded,
    FillChamfer,
    Stroke,
    StrokeChamfer,
    Arc,
    Polyline,
    Blit,
    GlyphRun,
    SlotHole>;

inline constexpr std::size_t kShapeKinds = std::variant_size_v<Shape>;

inline const char* shapeKindName(std::size_t index) {
  static constexpr const char* kNames[] = {
      "FillRect",
      "FillRounded",
      "FillChamfer",
      "Stroke",
      "StrokeChamfer",
      "Arc",
      "Polyline",
      "Blit",
      "GlyphRun",
      "SlotHole",
  };
  static_assert(sizeof(kNames) / sizeof(kNames[0]) == kShapeKinds);
  return index < kShapeKinds ? kNames[index] : "?";
}

struct SlotPlacement {
  std::uint32_t id = 0;
  Rect windowLogical{};
};

struct Hit {
  std::uint32_t semantic = 0;
  Rect bounds{};
  bool slot = false;
  bool opaque = true;
};

inline bool hasClip(const GroupParams& p) noexcept {
  return p.clip.size.x > 0.f && p.clip.size.y > 0.f;
}

inline float plateRadius(const Radius& r) noexcept {
  float m = r.lt;
  if (r.rt > m) {
    m = r.rt;
  }
  if (r.lb > m) {
    m = r.lb;
  }
  if (r.rb > m) {
    m = r.rb;
  }
  return m;
}

constexpr int kMaxBackdropPills = 4;

struct BackdropPill {
  Rect rect{};
  float radius = 0.f;
};

constexpr int kMaxGlassPills = 8;

enum class GlassPillKind : std::uint8_t { Capsule, Rounded };

struct GlassPill {
  Rect rect{};
  float radius = 0.f;
  float superellipseN = 4.f;
  GlassPillKind kind = GlassPillKind::Capsule;
};

struct GroupItem {
  enum Kind : std::uint8_t { Shape, Child } kind = Shape;
  std::uint32_t index = 0;
};

struct Group {
  GroupParams params;
  std::vector<Shape> shapes;
  std::vector<std::unique_ptr<Group>> children;
  std::vector<GroupItem> order;

  Group() = default;
  Group(Group&&) noexcept = default;
  Group& operator=(Group&&) noexcept = default;
  Group(const Group&);
  Group& operator=(const Group&);
};

struct Scene {
  Vec2 logicalSize;
  Group root;
  ImageStore images;
};

struct Stats {
  float encodeMs = 0;
  // Time blocked waiting for a swapchain drawable: the GPU or display
  // is behind when this approaches a vsync interval.
  float acquireMs = 0;
  // GPU execution time of the latest completed frame (usually an
  // earlier one); 0 when the backend does not measure it.
  float gpuMs = 0;
  unsigned draws = 0;
  unsigned instances = 0;
  unsigned isolateCount = 0;
  unsigned mergedGroupCount = 0;
  unsigned tileCount = 0;
  unsigned backdropCount = 0;
  unsigned glassPassCount = 0;
  unsigned glassPillCount = 0;
  unsigned reuseHits = 0;
  unsigned reuseMisses = 0;
  unsigned dirtyTiles = 0;
  unsigned shadowPassCount = 0;
  unsigned contentBlurCount = 0;
  // Encode cost by Shape alternative (index = Shape::index()); isolates excluded.
  unsigned shapeCount[kShapeKinds]{};
  unsigned shapeInstances[kShapeKinds]{};
  float shapeEncodeMs[kShapeKinds]{};
};

float snapBackdropSigma(float sigma);
int isolatePixelSize(float logical, float pixelRatio);
bool useHalfIsolate(const Group&);
int isolatePixelSizeFor(const Group&, float logical, float pixelRatio);
Rect backdropSurface(const Group&);
int collectBackdropPills(const Group&, BackdropPill* out);
void dropBackdropPills(Group&);
void clearBackdropParams(GroupParams&);
bool hasBackdrop(const Group&);
bool hasShadow(const Group&) noexcept;
bool hasContentBlur(const Group&) noexcept;
float snapShadowSigma(float sigma);
float snapContentSigma(float sigma);
Rect shadowContentSurface(const Group&);
void clearShadowParams(GroupParams&);
bool hasGlass(const Group&) noexcept;
bool isGlassContainer(const Group&) noexcept;
bool hasGlassWork(const Group&) noexcept;
Rect glassSurface(const Group&);
// Material on the group itself, else on its first glass child, else defaults.
const Glass& glassMaterial(const Group&);
// Fully reflective liquid metal: shaded from its environment, it samples
// no backdrop. Shaders derive the same flag from material + transmit.
bool glassOpaque(const Glass&);
// Effective backdrop blur radius for a material (0 for opaque metal).
float glassBlurRadius(const Glass&);
// Physical plate thickness, scaled with plate size like the isolate gutter.
float glassThicknessFor(const Glass& mat, Rect plateBounds);
// True when timeSec changes the rendered plate: the water ripple field or a
// drifting swell. Plates that ignore time skip hashing and uploading it, so
// raster reuse is not defeated for no visual difference.
bool glassAnimates(const Glass& mat);
// True when the water ripple field reads timeSec directly.
bool glassRipples(const Glass& mat);
// Swell phase (radians) at timeSec under mat.motion; exactly constant while
// the surface rests between surges, so resting frames reuse their raster.
// 0 for plates whose swell does not move.
float glassSwellPhase(const Glass& mat);
int collectGlassPills(const Group&, std::span<GlassPill> out);
void dropGlassPills(Group&);
void clearGlassParams(GroupParams&);
void stripGlassForIsolate(Group&);
bool needsIsolate(const Group&);
bool canMerge(const Group&);
bool blendCompatible(Blend parent, Blend child);

template <typename ShapeFn, typename ChildFn>
void visitGroup(const Group& g, ShapeFn&& onShape, ChildFn&& onChild) {
  if (g.order.empty()) {
    for (const Shape& s : g.shapes) {
      onShape(s);
    }
    for (const auto& child : g.children) {
      if (child) {
        onChild(*child);
      }
    }
    return;
  }
  for (const GroupItem& item : g.order) {
    if (item.kind == GroupItem::Shape) {
      if (item.index < g.shapes.size()) {
        onShape(g.shapes[item.index]);
      }
    } else if (item.index < g.children.size() && g.children[item.index]) {
      onChild(*g.children[item.index]);
    }
  }
}
std::unique_ptr<Group> cloneGroup(const Group&);
Group merge(Group, Stats* stats = nullptr);
Rect transformRect(const Mat4&, Rect);
bool isAxisAligned(const Mat4& t) noexcept;
bool invert2DAffine(const Mat4& t, Mat4& out) noexcept;
FillRect transformFill(const Mat4&, const FillRect&);
FillRounded transformRounded(const Mat4&, const FillRounded&);
FillChamfer transformChamfer(const Mat4&, const FillChamfer&);
Stroke transformStroke(const Mat4&, const Stroke&);
StrokeChamfer transformStrokeChamfer(const Mat4&, const StrokeChamfer&);
Arc transformArc(const Mat4&, const Arc&);
Polyline transformPolyline(const Mat4&, const Polyline&);
Blit transformBlit(const Mat4&, const Blit&);
GlyphRun transformGlyphs(const Mat4&, const GlyphRun&);
SlotHole transformSlot(const Mat4&, const SlotHole&);
Rect contentBounds(const Group&);
void appendTransformed(std::vector<Shape>& dst, const Mat4&, const Shape&);
Shape transformShape(const Mat4&, const Shape&);
bool hasSlotHole(const Group&);
void collectSlots(const Scene&, std::vector<SlotPlacement>* out);
bool hitTest(const Scene&, Vec2 logical, Hit* out);

constexpr int kTileSize = 32;

int coarseTileCount(Vec2 logicalSize, float pixelRatio);

}  // namespace glim::paint
