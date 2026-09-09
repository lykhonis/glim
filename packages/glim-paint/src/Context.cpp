#include <glim/paint/Context.h>

namespace glim::paint {

void Context::setSize(Vec2 size) {
    size_ = size;
}

void Context::beginFrame() {
    recording_ = true;
    state_ = State{};
    stack_.clear();
    scene_.logicalSize = size_;
    scene_.fills.clear();
}

void Context::finish() {
    recording_ = false;
}

void Context::setFillColor(Color color) {
    state_.fill = color;
}

void Context::fill(const Rect& rect) {
    if (!recording_) {
        return;
    }
    const Vec4 origin = state_.model * Vec4{rect.origin.x, rect.origin.y, 0, 1};
    const Vec4 corner = state_.model * Vec4{rect.origin.x + rect.size.x, rect.origin.y + rect.size.y, 0, 1};
    FillCommand cmd;
    cmd.rect.origin = {origin.x, origin.y};
    cmd.rect.size = {corner.x - origin.x, corner.y - origin.y};
    cmd.color = state_.fill;
    scene_.fills.push_back(cmd);
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

Mat4 Context::projection() const {
    return Mat4::orthoYDown(0, 0, size_.x, size_.y);
}

}  // namespace glim::paint
