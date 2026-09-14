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

    void setFill(Matter);
    void setFillColor(Color color) { setFill(Matter::solid(color)); }
    void setFillColor(std::uint32_t rgba) { setFillColor(Color{rgba}); }
    void fill(const Rect&);
    void fillRounded(const Rect&, Radius);
    void strokeRect(const Rect&, float width);
    void strokeRect(const Rect&, Radius, float width);
    void clipRect(const Rect&);
    void clipRect(const Rect&, Radius);
    std::uint32_t addImage(int width, int height, const std::uint8_t* rgba);
    void releaseImage(std::uint32_t id);
    const ImageStore& images() const { return images_; }
    void blit(const Rect& dst, Matter matter);
    void blit(const Rect& dst, std::uint32_t imageId) { blit(dst, Matter::sampled(imageId)); }
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
        Matter fill = Matter::solid(Color{0, 0, 0, 255});
    };

    Group* current();

    Vec2 size_{720, 480};
    State state_{};
    std::vector<State> stack_;
    std::vector<Group*> groupStack_;
    Scene scene_{};
    ImageStore images_{};
    bool recording_ = false;
};

}  // namespace glim::paint
