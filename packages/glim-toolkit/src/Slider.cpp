#include "glim/toolkit/elements/Slider.h"

#include <algorithm>
#include <cmath>

#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Theme.h"

namespace glim {
namespace toolkit {

namespace {

float snap(float v, float lo, float hi, float step) {
  if (step > 0.0f) v = lo + std::round((v - lo) / step) * step;
  return std::clamp(v, std::min(lo, hi), std::max(lo, hi));
}

}  // namespace

float Slider::at(const Box& box, float x) const {
  const float track = std::max(1.0f, box.w - kKnob);
  const float t = std::clamp((x - box.x - kKnob / 2) / track, 0.0f, 1.0f);
  return snap(lo + t * (hi - lo), lo, hi, step);
}

float Slider::stepped(float v, int steps) const {
  const float by = step > 0.0f ? step : (hi - lo) / 10.0f;
  return snap(v + static_cast<float>(steps) * by, lo, hi, step);
}

void Slider::paint(const Box& box, AnySink& sink, const Engine& e) const {
  const std::uint32_t ink = themeColor(e, kThemeInkOnLight);
  const float t = hi == lo ? 0.0f : (*value - lo) / (hi - lo);
  const float cy = box.y + box.h / 2;
  const float x0 = box.x + kKnob / 2;
  const float knobX = x0 + t * (box.w - kKnob);
  sink.fillRounded({x0, cy - 2, box.w - kKnob, 4}, 2, (ink & 0xffffff00u) | 0x40u);
  sink.fillRounded({x0, cy - 2, knobX - x0, 4}, 2, (ink & 0xffffff00u) | 0xc0u);
  sink.fillRounded(
    {knobX - kKnob / 2, cy - kKnob / 2, kKnob, kKnob}, static_cast<int>(kKnob / 2), ink
  );
}

bool Slider::handleAt(const Box& box, const Event& ev) const {
  const auto set = [&](float x) {
    const float next = at(box, x);
    if (next == *value) return false;
    value = next;
    return true;
  };
  if (auto d = ev.as<PointerDown>()) {
    set(d->get().x);
    return true;
  }
  if (auto m = ev.as<PointerMove>()) return m->get().pressed && set(m->get().x);
  if (ev.is<PointerUp>()) return true;
  return handle(ev);
}

bool Slider::handle(const Event& ev) const {
  int steps = 0;
  if (auto a = ev.as<Adjust>()) steps = a->get().step;
  if (auto k = ev.as<KeyPress>()) {
    if (k->get().key == Key::Right) steps = 1;
    if (k->get().key == Key::Left) steps = -1;
  }
  if (steps == 0) return false;
  const float next = stepped(*value, steps);
  if (next != *value) value = next;
  return true;
}

}  // namespace toolkit
}  // namespace glim
