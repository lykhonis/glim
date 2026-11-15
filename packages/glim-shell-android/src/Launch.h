#pragma once

#include <android_native_app_glue.h>

#include <string>
#include <vector>

namespace glim::shell::detail {

// argv for glim_app_main: "glim" plus the whitespace-split `args` intent
// extra (`am start ... --es args "--drive /path"`).
std::vector<std::string> launchArgs(android_app* app);
// stdout and stderr go nowhere on Android; send them to logcat tag "glim".
void forwardStdioToLogcat();

}  // namespace glim::shell::detail
