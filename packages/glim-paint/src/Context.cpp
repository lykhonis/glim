#include <glim/paint/Context.h>

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
}

void Context::finish() {
    recording_ = false;
    groupStack_.clear();
}

void Context::setFillColor(Color color) {
    state_.fill = color;
}

void Context::fill(const Rect& rect) {
    if (!recording_) {
        return;
    }
    current()->shapes.emplace_back(transformFill(state_.model, FillRect{rect, state_.fill}));
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
