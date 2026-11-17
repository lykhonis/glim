#pragma once

#include <glim/shell/TextInput.h>
#include <glim/toolkit/Focus.h>
#include <glim/toolkit/TextEdit.h>

#include <optional>

namespace glim::toolkit::host {

// The focused field for the shell's text input client; inactive without one.
shell::TextInputState shellTextInput(const std::optional<TextFocus>& focus);
TextChange toolkitText(const shell::TextInputChange& change);

}  // namespace glim::toolkit::host
