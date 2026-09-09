#pragma once

#include <memory>
#include <variant>
#include <vector>

#include <glim/math.h>

namespace glim::paint {

enum class Blend { SrcOver, Plus };

struct GroupParams {
    float opacity = 1.0f;
    Mat4 transform = Mat4::identity();
    Rect bounds{};
    bool isolate = false;
    Blend blend = Blend::SrcOver;
};

struct FillRect {
    Rect rect;
    Color color;
};

using Shape = std::variant<FillRect>;

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
FillRect transformFill(const Mat4&, const FillRect&);
Rect contentBounds(const Group&);

constexpr int kTileSize = 32;

int coarseTileCount(Vec2 logicalSize, float pixelRatio);

}  // namespace glim::paint
