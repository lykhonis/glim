#include "glim/toolkit/elements/TextField.h"

#include <algorithm>
#include <cmath>

#include "glim/toolkit/Focus.h"
#include "glim/toolkit/text/Text.h"
#include "text/Segment.h"

namespace glim {
namespace toolkit {

float FieldGeometry::offsetX(
  const Box& box, const std::string& text, std::size_t at, const Engine& e
) {
  const std::string prefix = text.substr(0, std::min(at, text.size()));
  const float w = text::measureText(prefix, {0, kUnbounded, 0, kUnbounded}, e).w;
  return box.x + kPadX + w;
}

float FieldGeometry::scroll(
  const Box& box, const TextEdit& edit, const TextRules& rules, const Engine& e
) {
  const std::string shown = shownText(edit.text, rules);
  const float caretX = offsetX(box, shown, shownOffset(edit.text, edit.selection.end(), rules), e);
  const float right = box.x + box.w - kPadX - kCaretWidth;
  return std::max(0.0f, caretX - right);
}

float FieldGeometry::textX(
  const Box& box, const TextEdit& edit, const TextRules& rules, std::size_t at, const Engine& e
) {
  const std::string shown = shownText(edit.text, rules);
  return offsetX(box, shown, shownOffset(edit.text, at, rules), e) - scroll(box, edit, rules, e);
}

Box FieldGeometry::caret(
  const Box& box, const TextEdit& edit, const TextRules& rules, const Engine& e
) {
  const float x = textX(box, edit, rules, edit.selection.end(), e);
  return {x, box.y + kPadY, kCaretWidth, Tokens::kTypeLine};
}

std::vector<TextStop> FieldGeometry::stops(
  const Box& box, const TextEdit& edit, const TextRules& rules, const Engine& e
) {
  const std::string& text = edit.text;
  std::vector<TextStop> out{{0, textX(box, edit, rules, 0, e)}};
  for (std::size_t at = text::nextCluster(text, 0); at > 0; at = text::nextCluster(text, at)) {
    out.push_back({at, textX(box, edit, rules, at, e)});
    if (at >= text.size()) break;
  }
  return out;
}

std::size_t FieldGeometry::offsetAt(
  const Box& box, const TextEdit& edit, const TextRules& rules, float x, const Engine& e
) {
  const std::vector<TextStop> all = stops(box, edit, rules, e);
  const auto nearest =
    std::min_element(all.begin(), all.end(), [x](const TextStop& a, const TextStop& b) {
      return std::abs(a.x - x) < std::abs(b.x - x);
    });
  return nearest->at;
}

Box FieldGeometry::highlight(
  const Box& box, const TextEdit& edit, const TextRules& rules, const Engine& e
) {
  if (edit.selection.length == 0) return {0, 0, 0, 0};
  const float a = textX(box, edit, rules, edit.selection.start, e);
  const float b = textX(box, edit, rules, edit.selection.end(), e);
  return {a, box.y + kPadY, b - a, Tokens::kTypeLine};
}

Box FieldGeometry::underline(
  const Box& box, const TextEdit& edit, const TextRules& rules, const Engine& e
) {
  if (!edit.marked) return {0, 0, 0, 0};
  const float a = textX(box, edit, rules, edit.marked->start, e);
  const float b = textX(box, edit, rules, edit.marked->end(), e);
  return {a, box.y + kPadY + Tokens::kTypeLine, b - a, 1.0f};
}

namespace {

template <class P>
const Engine& engineOf(const P& p) {
  static const Engine plain;
  return p.engine ? *p.engine : plain;
}

}  // namespace

void TextField::paint(const Box& box, AnySink& sink, const Engine& e) const {
  const TextEdit& edit = value.get();
  const std::uint32_t ink = themeColor(e, kThemeInkOnLight);
  const float size = themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);
  const float x = box.x + FieldGeometry::kPadX;
  const float y = box.y + FieldGeometry::kPadY;
  if (edit.text.empty()) {
    if (!placeholder.empty()) sink.text(x, y, placeholder, (ink & 0xffffff00u) | 0x80u, size);
    return;
  }
  sink.pushClip(box);
  const Box picked = FieldGeometry::highlight(box, edit, rules, e);
  if (picked.w > 0) sink.fill(picked, (ink & 0xffffff00u) | 0x40u);
  sink.text(
    x - FieldGeometry::scroll(box, edit, rules, e), y, shownText(edit.text, rules), ink, size
  );
  const Box under = FieldGeometry::underline(box, edit, rules, e);
  if (under.w > 0) sink.fill(under, ink);
  sink.pop();
}

bool TextField::handleAt(const Box& box, const Event& ev) const {
  if (auto in = ev.as<TextInput>()) {
    value = applyText(value.get(), *in->get().change, rules);
    return true;
  }
  if (auto probe = ev.as<TextProbe>()) {
    *probe->get().out = {.edit = value.get(), .field = box, .caret = {0, 0, 0, 0}, .rules = rules};
    return true;
  }
  if (auto d = ev.as<PointerDown>()) {
    const PointerDown& p = d->get();
    const std::size_t at = FieldGeometry::offsetAt(box, value.get(), rules, p.x, engineOf(p));
    value = pressText(value.get(), at, p.x, p.y, p.time, p.shift);
    return true;
  }
  if (auto m = ev.as<PointerMove>()) {
    if (!m->get().pressed) return false;
    const PointerMove& p = m->get();
    value =
      dragText(value.get(), FieldGeometry::offsetAt(box, value.get(), rules, p.x, engineOf(p)));
    return true;
  }
  return ev.is<PointerUp>() || ev.is<Activate>();
}

}  // namespace toolkit
}  // namespace glim
