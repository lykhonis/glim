#include "Glass.h"

#include <glim/math.h>
#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

void recordGlass(
    glim::paint::Context& context,
    glim::Vec2 size,
    float timeSeconds,
    glim::Rect safeArea,
    bool reduceTransparency
) {
  using glim::Radius;
  using glim::Rect;
  using glim::paint::Glass;
  using glim::paint::GlassVariant;
  using glim::paint::GroupParams;

  if (size.x < 1.f || size.y < 1.f) {
    return;
  }
  const Rect win = Rect::fromSize(size);
  const Rect area = safeArea.size.x > 0.f ? safeArea : win;
  // Reduce Motion freezes every animated input at t=0 (stripes, merge,
  // ripples).
  const float t = reduceTransparency ? 0.f : timeSeconds;
  const float m = std::min(area.size.x, area.size.y);
  const float pad = std::clamp(m * 0.055f, 14.f, 26.f);

  context.setFillColor(0x1a2228ff);
  context.fill(win);

  const float stripe = std::clamp(m * 0.22f, 96.f, 140.f);
  const float period = stripe * 4.f;
  const float scroll = std::fmod(t * 13.f, period);
  const float origin = -scroll;
  const int first = static_cast<int>(std::floor(-origin / stripe)) - 1;
  const int last =
      static_cast<int>(std::ceil((win.size.x - origin) / stripe)) + 1;
  for (int i = first; i <= last; ++i) {
    const float x = origin + static_cast<float>(i) * stripe;
    const int k = ((i % 4) + 4) % 4;
    const std::uint32_t c = k == 0   ? 0xd97b6bff
                            : k == 1 ? 0x2a9d8fff
                            : k == 2 ? 0xe9c46aff
                                     : 0x3d5a80ff;
    context.setFillColor(c);
    context.fill(Rect{{x, 0.f}, {stripe, win.size.y}});
  }
  // Tile grid (as in the reference): thin lines give the lenses
  // structure to bend, mirror, and focus in both axes.
  const float tile = stripe * 0.5f;
  context.setFillColor(0x1a222866);
  for (float gx = origin + static_cast<float>(first) * stripe;
       gx < win.size.x;
       gx += tile) {
    context.fill(Rect{{gx - 0.75f, 0.f}, {1.5f, win.size.y}});
  }
  for (float gy = std::fmod(win.size.y * 0.5f, tile) - tile;
       gy < win.size.y;
       gy += tile) {
    context.fill(Rect{{0.f, gy - 0.75f}, {win.size.x, 1.5f}});
  }

  const float gap = pad * 0.65f;
  const float pillH = std::clamp(m * 0.15f, 60.f, 88.f);
  const float inner = area.size.x - pad * 2.f;
  const float cellW = (inner - gap * 2.f) / 3.f;
  const float rowGap = pad * 0.9f;
  const float totalH = pillH * 2.f + rowGap;
  const float rowY = area.origin.y + (area.size.y - totalH) * 0.5f;
  const float rowY2 = rowY + pillH + rowGap;
  const Rect clearR{{area.origin.x + pad, rowY}, {cellW, pillH}};
  const Rect frostedR{
      {clearR.origin.x + cellW + gap, rowY}, {cellW, pillH}
  };

  // Merge pair shares the third cell; the animated gap stays inside it.
  const float circleS = pillH;
  const float rectW = circleS * 1.2f;
  const float mergeT =
      0.5f - 0.5f * std::cos(t * 0.35f);  // t=0: pair overlaps, neck
  const float mergeGapMax = cellW - circleS - rectW;
  const float mergeGap =
      -circleS * 0.1f + (mergeGapMax + circleS * 0.1f) * mergeT;
  const float mergeInner = circleS + mergeGap + rectW;
  const float mergeX = frostedR.origin.x + cellW + gap +
                       std::max(0.f, (cellW - mergeInner) * 0.5f);
  const Rect circleR{{mergeX, rowY}, {circleS, circleS}};
  const Rect rectR{
      {circleR.origin.x + circleS + mergeGap, rowY}, {rectW, circleS}
  };
  const Rect mergeBox{{mergeX, rowY}, {mergeInner, circleS}};

  Glass clear = Glass::clear();
  clear.flatten = reduceTransparency;
  GroupParams clearP;
  clearP.bounds = clearR;
  clearP.clip = clearR;
  clearP.clipRadius = Radius{pillH * 0.5f};
  clearP.glass = clear;
  context.pushGroup(clearP);
  context.popGroup();

  Glass frosted;
  frosted.variant = GlassVariant::Regular;
  frosted.flatten = reduceTransparency;
  GroupParams frostedP;
  frostedP.bounds = frostedR;
  frostedP.clip = frostedR;
  frostedP.clipRadius = Radius{pillH * 0.5f};
  frostedP.glass = frosted;
  context.pushGroup(frostedP);
  context.popGroup();

  // Second row: metal + water, centered under the glass row. No labels.
  const float row2W = cellW * 2.f + gap;
  const float row2X0 = area.origin.x + pad + (inner - row2W) * 0.5f;
  const Rect metalR{{row2X0, rowY2}, {cellW, pillH}};
  const Rect waterR{{row2X0 + cellW + gap, rowY2}, {cellW, pillH}};

  Glass metal = Glass::liquidMetal();
  metal.flatten = reduceTransparency;
  metal.timeSec = t;
  GroupParams metalP;
  metalP.bounds = metalR;
  metalP.clip = metalR;
  metalP.clipRadius = Radius{pillH * 0.5f};
  metalP.glass = metal;
  context.pushGroup(metalP);
  context.popGroup();

  Glass water = Glass::water();
  water.flatten = reduceTransparency;
  water.timeSec = t;
  GroupParams waterP;
  waterP.bounds = waterR;
  waterP.clip = waterR;
  waterP.clipRadius = Radius{pillH * 0.5f};
  waterP.glass = water;
  context.pushGroup(waterP);
  context.popGroup();

  Glass mergeG;
  mergeG.variant = GlassVariant::Regular;
  mergeG.mergeKPx = 16.f;
  mergeG.flatten = reduceTransparency;
  GroupParams box;
  box.bounds = mergeBox;
  box.glassContainer = true;
  box.glass = mergeG;
  context.pushGroup(box);
  GroupParams circ;
  circ.bounds = circleR;
  circ.clip = circleR;
  circ.clipRadius = Radius{circleS * 0.5f};
  circ.glass = mergeG;
  context.pushGroup(circ);
  context.popGroup();
  GroupParams rnd;
  rnd.bounds = rectR;
  rnd.clip = rectR;
  rnd.clipRadius = Radius{16.f};
  rnd.glass = mergeG;
  context.pushGroup(rnd);
  context.popGroup();
  context.popGroup();
}
