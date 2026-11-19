#include <glim/shell/ImeEdits.h>
#include <glim/shell/Window.h>

#include <algorithm>
#include <string>
#include <utility>

#include "Display.h"

#ifdef GLIM_HAS_TEXT_INPUT_V3
#include <wayland-client.h>

#include "text-input-unstable-v3-client-protocol.h"
#endif

namespace glim::shell {

namespace {

// The seat's IME client. Events build up an update the compositor applies
// atomically on done; the field state is what the toolkit last published.
struct Ime {
  TextInputState state;
  TextInputCallback callback;
  ImeUpdate pending;
  bool entered = false;
  bool enabled = false;
#ifdef GLIM_HAS_TEXT_INPUT_V3
  zwp_text_input_v3* input = nullptr;
#endif
};

thread_local Ime gIme;

#ifdef GLIM_HAS_TEXT_INPUT_V3
// text-input-v3 caps surrounding text at 4000 bytes; a longer value
// sends none rather than a window the IME would misplace.
constexpr std::size_t kSurroundingMax = 4000;

void sendState() {
  zwp_text_input_v3* in = gIme.input;
  const TextInputState& s = gIme.state;
  if (!in || !gIme.entered) return;
  if (!s.active) {
    if (gIme.enabled) {
      zwp_text_input_v3_disable(in);
      zwp_text_input_v3_commit(in);
      gIme.enabled = false;
    }
    return;
  }
  if (!gIme.enabled) {
    zwp_text_input_v3_enable(in);
    gIme.enabled = true;
  }
  if (!s.secure && s.text.size() < kSurroundingMax) {
    const auto anchor = static_cast<int32_t>(s.selection.start);
    const auto cursor = static_cast<int32_t>(s.selection.start + s.selection.length);
    zwp_text_input_v3_set_surrounding_text(in, s.text.c_str(), cursor, anchor);
  }
  zwp_text_input_v3_set_content_type(
    in,
    s.secure
      ? ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA | ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT
      : ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
    s.secure ? ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD : ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL
  );
  zwp_text_input_v3_set_cursor_rectangle(
    in,
    static_cast<int32_t>(s.caret.origin.x),
    static_cast<int32_t>(s.caret.origin.y),
    static_cast<int32_t>(s.caret.size.x),
    static_cast<int32_t>(s.caret.size.y)
  );
  zwp_text_input_v3_commit(in);
}

void enter(void*, zwp_text_input_v3*, wl_surface*) {
  gIme.entered = true;
  gIme.enabled = false;
  sendState();
}

void leave(void*, zwp_text_input_v3*, wl_surface*) {
  gIme.entered = false;
  gIme.enabled = false;
}

void preedit(void*, zwp_text_input_v3*, const char* text, int32_t begin, int32_t) {
  gIme.pending.preedit = text ? text : "";
  gIme.pending.preeditCursor = begin;
}

void commitString(void*, zwp_text_input_v3*, const char* text) {
  gIme.pending.commit = text ? text : "";
}

void deleteSurrounding(void*, zwp_text_input_v3*, uint32_t before, uint32_t after) {
  gIme.pending.deleteBefore = before;
  gIme.pending.deleteAfter = after;
}

void done(void*, zwp_text_input_v3*, uint32_t) {
  ImeUpdate update = std::exchange(gIme.pending, {});
  if (!gIme.state.active || !gIme.callback) return;
  for (const TextInputChange& c : imeUpdate(gIme.state, std::move(update))) gIme.callback(c);
}

const zwp_text_input_v3_listener kListener = [] {
  zwp_text_input_v3_listener l{};
  l.enter = enter;
  l.leave = leave;
  l.preedit_string = preedit;
  l.commit_string = commitString;
  l.delete_surrounding_text = deleteSurrounding;
  l.done = done;
  return l;
}();

void ensureInput() {
  detail::Wayland& wl = detail::wayland();
  if (gIme.input || !wl.textInputs || !wl.seat) return;
  gIme.input = zwp_text_input_manager_v3_get_text_input(wl.textInputs, wl.seat);
  zwp_text_input_v3_add_listener(gIme.input, &kListener, nullptr);
}
#endif

}  // namespace

namespace detail {

void textInputDisconnect() {
#ifdef GLIM_HAS_TEXT_INPUT_V3
  if (gIme.input) zwp_text_input_v3_destroy(gIme.input);
#endif
  gIme = {};
}

}  // namespace detail

void Window::setTextInput(TextInputState state) {
  const bool same = sameTextInput(state, gIme.state);
  gIme.state = std::move(state);
  if (same) return;
#ifdef GLIM_HAS_TEXT_INPUT_V3
  sendState();
#endif
}

void Window::setTextInputCallback(TextInputCallback callback) {
  gIme.callback = std::move(callback);
#ifdef GLIM_HAS_TEXT_INPUT_V3
  ensureInput();
#endif
}

}  // namespace glim::shell
