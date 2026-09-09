#include <glim/paint/Context.h>

#include <cmath>

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

void recordHello(Context& context, Vec2 size, float timeSeconds) {
    context.setSize(size);
    context.beginFrame();

    context.setFillColor(0x243038ff);
    context.fill(Rect::fromSize(size));

    context.save();
    context.translate({100.f, 50.f});
    context.setFillColor(0xac6363ff);
    context.fill(Rect::fromSize({400.f, 300.f}));
    context.restore();

    const float bob = std::sin(timeSeconds * 1.4f) * 10.f;
    const float sideX = std::max(520.f, size.x - 220.f);
    context.save();
    context.translate({sideX, 70.f + bob});
    context.setFillColor(0x3d7ea6ff);
    context.fill(Rect::fromSize({180.f, 200.f}));
    context.restore();

    context.save();
    context.translate({140.f, 80.f});
    context.setFillColor(0xf0d5a8ff);
    context.fill(Rect::fromSize({80.f, 48.f}));
    context.restore();

    GroupParams glass;
    glass.opacity = 0.42f;
    glass.bounds = Rect{{0, 0}, {size.x, 96.f}};
    glass.transform = Mat4::translate(0, size.y - 96.f);
    context.pushGroup(glass);
    context.setFillColor(0xe8eef4ff);
    context.fill(Rect::fromSize({size.x, 96.f}));
    context.setFillColor(0x2a9d8fff);
    context.fill(Rect{{24.f, 28.f}, {160.f, 40.f}});
    context.popGroup();

    context.finish();
}

}  // namespace glim::paint
