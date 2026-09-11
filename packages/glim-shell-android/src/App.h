#pragma once

#include <android/native_window.h>
#include <android_native_app_glue.h>

namespace glim::shell::detail {

android_app* androidApp();
void setAndroidApp(android_app* app);
ANativeWindow* nativeWindow();
float pixelRatio();
int drawableWidth();
int drawableHeight();

}  // namespace glim::shell::detail
