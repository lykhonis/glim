#include <glim/shell/ImeEdits.h>
#include <glim/shell/Window.h>

#include <string>
#include <utility>
#include <vector>

#include "Shown.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace glim::shell {

namespace {

// The focused field as last published, and where its edits go.
struct Ime {
  TextInputState state;
  TextInputCallback callback;
  bool focused = false;
};

Ime& ime() {
  static Ime i;
  return i;
}

// What the bridge reports, in UTF-16 against the published field.
enum class Edit { Commit, Compose, Select, DeleteBackward, DeleteForward, EndCompose };

std::vector<TextInputChange> changes(Edit kind, std::string text, int a, int b) {
  const TextInputState& s = ime().state;
  switch (kind) {
    case Edit::Commit:
      return imeCommit(s, std::move(text));
    case Edit::Compose:
      return imeCompose(s, std::move(text));
    case Edit::Select:
      return imeSelect(s, static_cast<std::size_t>(a), static_cast<std::size_t>(b));
    case Edit::DeleteBackward:
      return {{.op = TextInputOp::DeleteBackward}};
    case Edit::DeleteForward:
      return {{.op = TextInputOp::DeleteForward}};
    case Edit::EndCompose:
      return s.marked ? std::vector<TextInputChange>{{.op = TextInputOp::Unmark}}
                      : std::vector<TextInputChange>{};
  }
  return {};
}

}  // namespace

namespace detail {

bool textInputFocused() { return ime().focused; }

}  // namespace detail

}  // namespace glim::shell

#if defined(__EMSCRIPTEN__)
extern "C" {

EMSCRIPTEN_KEEPALIVE void glim_web_text(int kind, const char* text, int a, int b) {
  auto& i = glim::shell::ime();
  if (!i.state.active || !i.callback) return;
  for (const auto& c :
       glim::shell::changes(static_cast<glim::shell::Edit>(kind), text ? text : "", a, b)) {
    i.callback(c);
  }
}

EMSCRIPTEN_KEEPALIVE void glim_web_text_focus(int focused) {
  glim::shell::ime().focused = focused != 0;
}

// Defined in text_input.lib.js.
void glim_web_text_publish(
  int active, int secure, const char* text, int start, int end, float x, float y, float h
);

}  // extern "C"
#endif

namespace glim::shell {

void Window::setTextInput(TextInputState state) {
  Ime& i = ime();
  if (sameTextInput(state, i.state)) return;
  i.state = std::move(state);
#if defined(__EMSCRIPTEN__)
  const TextInputState& s = i.state;
  const auto start = static_cast<int>(utf16Units(s.text, s.selection.start));
  const auto end = static_cast<int>(utf16Units(s.text, s.selection.start + s.selection.length));
  glim_web_text_publish(
    s.active,
    s.secure,
    s.text.c_str(),
    start,
    end,
    s.caret.origin.x,
    s.caret.origin.y,
    s.caret.size.y
  );
#endif
}

void Window::setTextInputCallback(TextInputCallback callback) {
  ime().callback = std::move(callback);
}

}  // namespace glim::shell
