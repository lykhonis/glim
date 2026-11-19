#include <glim/shell/Clipboard.h>

// Shells whose platform clipboard is not wired yet (N8.4): callers see
// Unsupported and can say so.
namespace glim::shell {

ClipboardText readClipboard() { return {}; }

ClipboardStatus writeClipboard(std::string_view) { return ClipboardStatus::Unsupported; }

}  // namespace glim::shell
