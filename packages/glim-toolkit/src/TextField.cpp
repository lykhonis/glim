#include "glim/toolkit/elements/TextField.h"

#include <algorithm>

#include "glim/toolkit/Focus.h"
#include "glim/toolkit/text/Text.h"

namespace glim {
namespace toolkit {

float FieldGeometry::offsetX(
  const Box& box, const std::string& text, std::size_t at, const Engine& e
) {
  const std::string prefix = text.substr(0, std::min(at, text.size()));
  const float w = text::measureText(prefix, {0, kUnbounded, 0, kUnbounded}, e).w;
  return box.x + kPadX + w;
}

Box FieldGeometry::caret(
  const Box& box, const TextEdit& edit, const TextRules& rules, const Engine& e
) {
  const std::size_t at = shownOffset(edit.text, edit.selection.end(), rules);
  const float x = offsetX(box, shownText(edit.text, rules), at, e);
  return {x, box.y + kPadY, kCaretWidth, Tokens::kTypeLine};
}

Box FieldGeometry::underline(
  const Box& box, const TextEdit& edit, const TextRules& rules, const Engine& e
) {
  if (!edit.marked) return {0, 0, 0, 0};
  const std::string shown = shownText(edit.text, rules);
  const float a = offsetX(box, shown, shownOffset(edit.text, edit.marked->start, rules), e);
  const float b = offsetX(box, shown, shownOffset(edit.text, edit.marked->end(), rules), e);
  return {a, box.y + kPadY + Tokens::kTypeLine, b - a, 1.0f};
}

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
  sink.text(x, y, shownText(edit.text, rules), ink, size);
  const Box under = FieldGeometry::underline(box, edit, rules, e);
  if (under.w > 0) sink.fill(under, ink);
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
  return ev.is<PointerDown>() || ev.is<Activate>();
}

}  // namespace toolkit
}  // namespace glim
