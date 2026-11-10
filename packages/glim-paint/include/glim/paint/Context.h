#pragma once

#include <glim/math.h>
#include <glim/paint/Scene.h>

#include <cstdint>
#include <vector>

namespace glim::paint {

struct TextSize {
  float width = 0;
  float ascent = 0;
  float descent = 0;
  float height = 0;
};

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
  void fillChamfer(const Rect&, float cut);
  void strokeRect(const Rect&, float width);
  void strokeRect(const Rect&, Radius, float width);
  void strokeChamfer(const Rect&, float cut, float width);
  void arc(const Arc&);
  void polyline(const Polyline&);
  void clipRect(const Rect&);
  void clipRect(const Rect&, Radius);
  std::uint32_t addImage(int width, int height, const std::uint8_t* rgba);
  std::uint32_t wrapNativeTexture(
      void* native, int width, int height, SampleFormat format = SampleFormat::Rgba8Unorm
  );
  void releaseImage(std::uint32_t id);
  const ImageStore& images() const { return images_; }
  void blit(const Rect& dst, Matter matter);
  void blit(const Rect& dst, std::uint32_t imageId) { blit(dst, Matter::sampled(imageId)); }
  void text(Vec2 origin, const char* latin, float sizePx);
  void textStyled(Vec2 origin, const char* latin, float sizePx, float condense, float weightPx);
  TextSize measureText(const char* latin, float sizePx) const;
  TextSize measureTextStyled(const char* latin, float sizePx, float condense) const;
  void slot(const Rect&, std::uint32_t id);
  bool sawSlots() const { return sawSlots_; }
  void setSemantic(std::uint32_t id);
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
  std::uint32_t fontAtlasId_ = 0;
  bool recording_ = false;
  bool sawSlots_ = false;
};

}  // namespace glim::paint
