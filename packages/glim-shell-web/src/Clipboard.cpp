#include <glim/shell/Clipboard.h>

#include <string>

#if defined(__EMSCRIPTEN__)
// Defined in text_input.lib.js.
extern "C" int glim_web_clipboard_write(const char* text);
#endif

namespace glim::shell {

// The browser reads the clipboard only asynchronously: reads say so, and
// pasting goes through the text bridge's paste event instead.
ClipboardText readClipboard() { return {}; }

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
