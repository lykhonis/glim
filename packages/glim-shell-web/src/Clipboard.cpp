#include <glim/shell/Clipboard.h>

#include <cstdlib>
#include <string>

#if defined(__EMSCRIPTEN__)
// Defined in text_input.lib.js.
extern "C" int glim_web_clipboard_write(const char* text);
extern "C" char* glim_web_clipboard_read();
#endif

namespace glim::shell {

// The browser reads the clipboard only asynchronously, so reads answer from
// the text bridge's mirror of it; Unsupported until the page has seen any.
ClipboardText readClipboard() {
#if defined(__EMSCRIPTEN__)
  char* text = glim_web_clipboard_read();
  if (!text) return {};
  ClipboardText out{.status = *text ? ClipboardStatus::Ok : ClipboardStatus::Empty, .text = text};
  std::free(text);
  return out;
#else
  return {};
#endif
}

ClipboardStatus writeClipboard(std::string_view text) {
#if defined(__EMSCRIPTEN__)
  const std::string s(text);
  return glim_web_clipboard_write(s.c_str()) ? ClipboardStatus::Ok : ClipboardStatus::Unsupported;
#else
  (void)text;
  return ClipboardStatus::Unsupported;
#endif
}

}  // namespace glim::shell
