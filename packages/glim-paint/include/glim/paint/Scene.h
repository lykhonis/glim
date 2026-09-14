#pragma once

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include <glim/math.h>

namespace glim::paint {

enum class Blend { SrcOver, Plus };

enum class MatterKind { Solid, Sampled };

constexpr int kMaxImageSide = 4096;
constexpr std::uint64_t kMaxImageBytes = 16ull * 1024ull * 1024ull;

struct StoredImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

// CPU pixels keyed by paint-level id. No gpu::Handle. Shared across Context/Scene copies.
struct ImageStore {
    ImageStore();
    std::uint32_t add(int width, int height, const std::uint8_t* rgba);
    void release(std::uint32_t id);
    const StoredImage* get(std::uint32_t id) const;

private:
    struct Data {
        std::vector<StoredImage> slots{StoredImage{}};
    };
    std::shared_ptr<Data> data_;
};

// Pigment for a Shape. Solid or sampled (imageId). Foreign later.
struct Matter {
    MatterKind kind = MatterKind::Solid;
    Color color{};
    std::uint32_t imageId = 0;
    Rect uv{{0.f, 0.f}, {1.f, 1.f}};

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
};

struct GroupParams {
    float opacity = 1.0f;
    Mat4 transform = Mat4::identity();
    Rect bounds{};
    bool isolate = false;
    Blend blend = Blend::SrcOver;
};

struct FillRect {
    Rect rect;
    Matter matter;
};

struct Blit {
    Rect rect;
    Matter matter;
};

using Shape = std::variant<FillRect, Blit>;

struct Group {
    GroupParams params;
    std::vector<Shape> shapes;
    std::vector<std::unique_ptr<Group>> children;

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
};

bool needsIsolate(const Group&);
bool canMerge(const Group&);
std::unique_ptr<Group> cloneGroup(const Group&);
Group merge(Group, Stats* stats = nullptr);
Rect transformRect(const Mat4&, Rect);
FillRect transformFill(const Mat4&, const FillRect&);
Blit transformBlit(const Mat4&, const Blit&);
Rect contentBounds(const Group&);
void appendTransformed(std::vector<Shape>& dst, const Mat4&, const Shape&);

constexpr int kTileSize = 32;

int coarseTileCount(Vec2 logicalSize, float pixelRatio);

}  // namespace glim::paint
