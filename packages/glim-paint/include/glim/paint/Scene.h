#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <glim/math.h>

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

// CPU pixels keyed by paint-level id. No gpu::Handle. Shared across Context/Scene copies.
struct ImageStore {
    ImageStore();
    std::uint32_t add(int width, int height, const std::uint8_t* rgba);
    std::uint32_t wrap(void* native, int width, int height, SampleFormat format);
    void release(std::uint32_t id);
    const StoredImage* get(std::uint32_t id) const;

private:
    struct Data {
        std::vector<StoredImage> slots{StoredImage{}};
    };
    std::shared_ptr<Data> data_;
};

enum class GlassVariant { Regular, Clear, Identity };

struct Glass {
    GlassVariant variant = GlassVariant::Regular;
    Color tint{};
    bool interactive = false;
    float ior = 1.45f;
    float thicknessPx = 24.f;
    float mergeKPx = 16.f;
    float refDistance = 0.35f;
    float dispersion = 0.12f;
    bool flatten = false;

    static Glass clear() {
        Glass g;
        g.variant = GlassVariant::Clear;
        g.thicknessPx = 16.f;
        g.ior = 1.33f;
        g.dispersion = 0.08f;
        return g;
    }
};

// Pigment for a Shape. Solid, sampled, foreign (imageId), or gradient.
// Gradient geometry lives in the same logical space as the Shape rect and is
// transformed with it. Stops are sorted by offset by the factories.
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
    // Insertion sort by offset; stable for equal offsets.
    for (int i = 1; i < gradientStopCount; ++i) {
        GradientStop key = gradientStops[i];
        int j = i - 1;
        while (j >= 0 && gradientStops[j].offset > key.offset) {
            gradientStops[j + 1] = gradientStops[j];
            --j;
        }
        gradientStops[j + 1] = key;
    }
    // Legacy single-color paths (Blit tint, GlyphRun color) degrade to the
    // first stop instead of black.
    color = gradientStops[0].color;
}

struct GroupParams {
    float opacity = 1.0f;
    Mat4 transform = Mat4::identity();
    Rect bounds{};
    bool isolate = false;
    Blend blend = Blend::SrcOver;
    Rect clip{};
    Radius clipRadius{};
    float backdropBlur = 0.f;
    float backdropBend = 0.f;
    float backdropMerge = 0.f;
    float backdropPress = 0.f;
    float backdropLightX = 0.35f;
    float backdropLightY = 0.8f;
    float backdropLightZ = 0.5f;
    bool backdropFlat = false;
    std::optional<Glass> glass;
    bool glassContainer = false;
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

struct Stroke {
    Rect rect;
    Radius radius;
    float width = 1.f;
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

// Hosted native hole. Occupies a rect; Renderer emits no coverage. Illegal under needsIsolate.
struct SlotHole {
    Rect rect{};
    std::uint32_t id = 0;
};

using Shape = std::variant<FillRect, FillRounded, Stroke, Blit, GlyphRun, SlotHole>;

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
    unsigned draws = 0;
    unsigned instances = 0;
    unsigned isolateCount = 0;
    unsigned mergedGroupCount = 0;
    unsigned tileCount = 0;
    unsigned backdropCount = 0;
    unsigned glassPassCount = 0;
    unsigned glassPillCount = 0;
    // Raster reuse (Renderer-internal; Phase 2+). Zero when reuse is off.
    unsigned reuseHits = 0;    // cached encode reused (exact or translation-only)
    unsigned reuseMisses = 0;  // full merge+encode performed
    unsigned dirtyTiles = 0;   // tiles re-encoded this frame (tile dirtying, later)
};

float snapBackdropSigma(float sigma);
int isolatePixelSize(float logical, float pixelRatio);
Rect backdropSurface(const Group&);
int collectBackdropPills(const Group&, BackdropPill* out);
void dropBackdropPills(Group&);
void clearBackdropParams(GroupParams&);
bool hasBackdrop(const Group&);
bool hasGlass(const Group&) noexcept;
bool isGlassContainer(const Group&) noexcept;
bool hasGlassWork(const Group&) noexcept;
Rect glassSurface(const Group&);
int collectGlassPills(const Group&, GlassPill* out);
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
FillRect transformFill(const Mat4&, const FillRect&);
FillRounded transformRounded(const Mat4&, const FillRounded&);
Stroke transformStroke(const Mat4&, const Stroke&);
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
