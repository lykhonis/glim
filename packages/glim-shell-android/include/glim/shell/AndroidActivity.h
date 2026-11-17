#pragma once

#include <jni.h>

namespace glim::shell {

// The hosting Activity, for app-owned Java glue. Empty before android_main runs.
struct AndroidActivity {
  JavaVM* vm = nullptr;
  jobject activity = nullptr;
};

AndroidActivity androidActivity();

}  // namespace glim::shell
