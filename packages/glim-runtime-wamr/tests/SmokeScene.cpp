#include "SmokeScene.h"

#include <glim/paint/Context.h>

namespace glim::smoke {

paint::Scene scene() {
  paint::Context ctx;
  ctx.setSize({kWidth, kHeight});
  ctx.beginFrame();
  ctx.setFillColor(0x2050a0ff);
  ctx.fill(Rect::fromSize({kWidth, kHeight}));

  const paint::GradientStop stops[] = {{0.f, Color{0xff8000ff}}, {1.f, Color{0x00c0ffff}}};
  ctx.setFill(paint::Matter::linearGradient({0, 0}, {kWidth, 0}, stops, 2));
  ctx.fillRounded(Rect{{4, 4}, {88, 20}}, Radius{6.f});

  paint::GroupParams fade;
  fade.opacity = 0.5f;
  fade.bounds = Rect{{60, 28}, {30, 30}};
  fade.shadow = paint::ShadowBlur{4.f, {2, 2}, Color{0x00000080}, 0.f};
  ctx.pushGroup(fade);
  ctx.setFillColor(0x30ff60ff);
  ctx.fillRounded(Rect{{60, 28}, {30, 30}}, Radius{8.f});
  ctx.popGroup();

  paint::GroupParams glass;
  glass.lens = paint::Lens{};
  glass.bounds = Rect{{20, 48}, {56, 20}};
  glass.clipRadius = Radius{10.f};
  ctx.pushGroup(glass);
  ctx.popGroup();
  ctx.finish();
  return ctx.scene();
}

}  // namespace glim::smoke
