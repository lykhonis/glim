#pragma once

#include <cstdint>
#include <vector>

#include <glim/math.h>

namespace glim::paint {

constexpr int kSdfPad = 5;
constexpr int kSdfOnEdge = 128;
constexpr float kSdfBakePx = 32.f;

struct FontGlyph {
    bool present = false;
    float xoff = 0;
    float yoff = 0;
    float w = 0;
    float h = 0;
    float advance = 0;
    Rect uv{};
};

struct FontAtlas {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
    float bakePx = kSdfBakePx;
    float ascent = 0;
    float descent = 0;
    float lineHeight = 0;
    FontGlyph glyphs[128]{};
};

const FontAtlas& latinAtlas();

}  // namespace glim::paint
