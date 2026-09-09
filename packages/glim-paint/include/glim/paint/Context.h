#pragma once

#include <cstdint>
#include <vector>

#include <glim/math.h>
#include <glim/paint/Scene.h>

namespace glim::paint {

class Context {
public:
    void setSize(Vec2 size);
    Vec2 size() const { return size_; }

    void beginFrame();
    void finish();

    void setFillColor(Color);
    void setFillColor(std::uint32_t rgba) { setFillColor(Color{rgba}); }
    void fill(const Rect&);
    void translate(Vec2);
    void save();
    void restore();

    void pushGroup(const GroupParams&);
    void popGroup();

    const Scene& scene() const { return scene_; }
    Mat4 projection() const;

private:
    struct State {
        Mat4 model = Mat4::identity();
        Color fill{0, 0, 0, 255};
    };

    Group* current();

    Vec2 size_{720, 480};
    State state_{};
    std::vector<State> stack_;
    std::vector<Group*> groupStack_;
    Scene scene_{};
    bool recording_ = false;
};

void recordHello(Context& context, Vec2 size, float timeSeconds);

}  // namespace glim::paint
