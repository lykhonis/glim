#pragma once

#include <string>
#include <string_view>

namespace glim::shell {

// The system clipboard's text. Every shell defines these; a shell without
// clipboard access answers Unsupported, never a silent empty read or a
// dropped write. Main thread only.
enum class ClipboardStatus { Ok, Empty, Unsupported };

struct ClipboardText {
  ClipboardStatus status = ClipboardStatus::Unsupported;
  std::string text;
};

ClipboardText readClipboard();
ClipboardStatus writeClipboard(std::string_view text);

}  // namespace glim::shell
