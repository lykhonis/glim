#include <glim/paint/Context.h>

#include "Font.h"

#include <glim/assert.h>

#include <algorithm>
#include <cstdint>

namespace glim::paint {

void Context::setSize(Vec2 size) {
    size_ = size;
}

Group* Context::current() {
    return groupStack_.empty() ? &scene_.root : groupStack_.back();
}

void Context::beginFrame() {
    recording_ = true;
    state_ = State{};
    stack_.clear();
    groupStack_.clear();
    scene_.logicalSize = size_;
    scene_.root = Group{};
    scene_.images = images_;
}

void Context::finish() {
    recording_ = false;
    groupStack_.clear();
    scene_.images = images_;
}

void Context::setFill(Matter matter) {
    state_.fill = matter;
}

void Context::fill(const Rect& rect) {
    if (!recording_) {
        return;
    }
    current()->shapes.emplace_back(
        transformFill(state_.model, FillRect{rect, Matter::solid(state_.fill.color)}));
}

void Context::fillRounded(const Rect& rect, Radius radius) {
    if (!recording_) {
        return;
    }
    current()->shapes.emplace_back(transformRounded(
        state_.model, FillRounded{rect, radius, Matter::solid(state_.fill.color)}));
}

void Context::strokeRect(const Rect& rect, float width) {
    strokeRect(rect, Radius{}, width);
}

void Context::strokeRect(const Rect& rect, Radius radius, float width) {
    if (!recording_ || width <= 0.f) {
        return;
    }
    current()->shapes.emplace_back(transformStroke(
        state_.model, Stroke{rect, radius, width, Matter::solid(state_.fill.color)}));
}

void Context::clipRect(const Rect& rect) {
    clipRect(rect, Radius{});
}

void Context::clipRect(const Rect& rect, Radius radius) {
    if (!recording_) {
        return;
    }
    Group* g = current();
    const Rect xf = transformRect(state_.model, rect);
    const FillRounded scaled =
        transformRounded(state_.model, FillRounded{rect, radius, Matter::solid(Color{})});
    if (!hasClip(g->params)) {
        g->params.clip = xf;
        g->params.clipRadius = scaled.radius;
        return;
    }
    const float x0 = std::max(g->params.clip.origin.x, xf.origin.x);
    const float y0 = std::max(g->params.clip.origin.y, xf.origin.y);
    const float x1 =
        std::min(g->params.clip.origin.x + g->params.clip.size.x, xf.origin.x + xf.size.x);
    const float y1 =
        std::min(g->params.clip.origin.y + g->params.clip.size.y, xf.origin.y + xf.size.y);
    if (x1 <= x0 || y1 <= y0) {
        g->params.clip = {};
        g->params.clipRadius = {};
        return;
    }
    g->params.clip = {{x0, y0}, {x1 - x0, y1 - y0}};
    g->params.clipRadius = {};
}

std::uint32_t Context::addImage(int width, int height, const std::uint8_t* rgba) {
    return images_.add(width, height, rgba);
}

std::uint32_t Context::wrapNativeTexture(void* native, int width, int height, SampleFormat format) {
    return images_.wrap(native, width, height, format);
}

void Context::releaseImage(std::uint32_t id) {
    images_.release(id);
}

void Context::blit(const Rect& dst, Matter matter) {
    if (!recording_) {
        return;
    }
    if (matter.kind == MatterKind::Solid) {
        const StoredImage* img = images_.get(matter.imageId);
        matter.kind = (img && img->foreign) ? MatterKind::Foreign : MatterKind::Sampled;
    }
    current()->shapes.emplace_back(transformBlit(state_.model, Blit{dst, matter}));
}

TextSize Context::measureText(const char* latin, float sizePx) const {
    TextSize out;
    const FontAtlas& atlas = latinAtlas();
    if (sizePx <= 0.f || atlas.bakePx <= 0.f) {
        return out;
    }
    const float s = sizePx / atlas.bakePx;
    out.ascent = atlas.ascent * s;
    out.descent = -atlas.descent * s;
    out.height = atlas.lineHeight * s;
    if (!latin) {
        return out;
    }
    float w = 0.f;
    for (const char* p = latin; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c >= 128 || !atlas.glyphs[c].present) {
            continue;
        }
        w += atlas.glyphs[c].advance * s;
    }
    out.width = w;
    return out;
}

void Context::text(Vec2 origin, const char* latin, float sizePx) {
    if (!recording_ || !latin || sizePx <= 0.f) {
        return;
    }
    const FontAtlas& atlas = latinAtlas();
    if (atlas.width <= 0 || atlas.rgba.empty() || atlas.bakePx <= 0.f) {
        return;
    }
    if (fontAtlasId_ == 0) {
        fontAtlasId_ = images_.add(atlas.width, atlas.height, atlas.rgba.data());
        if (fontAtlasId_ == 0) {
            return;
        }
    }
    GlyphRun run;
    run.origin = origin;
    run.sizePx = sizePx;
    run.color = state_.fill.color;
    run.imageId = fontAtlasId_;
    const float s = sizePx / atlas.bakePx;
    float pen = origin.x;
    for (const char* p = latin; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c >= 128 || !atlas.glyphs[c].present) {
            continue;
        }
        const FontGlyph& g = atlas.glyphs[c];
        if (g.w > 0.f && g.h > 0.f) {
            GlyphQuad q;
            q.dest = {{pen + g.xoff * s, origin.y + g.yoff * s}, {g.w * s, g.h * s}};
            q.uv = g.uv;
            run.glyphs.push_back(q);
        }
        pen += g.advance * s;
    }
    if (run.glyphs.empty()) {
        return;
    }
    current()->shapes.emplace_back(transformGlyphs(state_.model, run));
}

void Context::slot(const Rect& rect, std::uint32_t id) {
    if (!recording_ || id == 0) {
        return;
    }
    GLIM_ASSERT(!needsIsolate(*current()), "SlotHole illegal under needsIsolate");
    for (const Group* g : groupStack_) {
        GLIM_ASSERT(g && !needsIsolate(*g), "SlotHole illegal under needsIsolate");
    }
    current()->shapes.emplace_back(transformSlot(state_.model, SlotHole{rect, id}));
}

void Context::translate(Vec2 offset) {
    state_.model = state_.model * Mat4::translate(offset.x, offset.y);
}

void Context::save() {
    stack_.push_back(state_);
}

void Context::restore() {
    if (stack_.empty()) {
        return;
    }
    state_ = stack_.back();
    stack_.pop_back();
}

void Context::pushGroup(const GroupParams& params) {
    if (!recording_) {
        return;
    }
    Group child;
    child.params = params;
    child.params.transform = state_.model * params.transform;
    current()->children.push_back(std::make_unique<Group>(std::move(child)));
    groupStack_.push_back(current()->children.back().get());
    save();
    state_.model = Mat4::identity();
}

void Context::popGroup() {
    if (groupStack_.empty()) {
        return;
    }
    groupStack_.pop_back();
    restore();
}

Mat4 Context::projection() const {
    return Mat4::orthoYDown(0, 0, size_.x, size_.y);
}

}  // namespace glim::paint
