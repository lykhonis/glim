#pragma once

#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <jni.h>

namespace glim::shell::detail {

android_app* androidApp();
void setAndroidApp(android_app* app);
ANativeWindow* nativeWindow();
float pixelRatio();
int drawableWidth();
int drawableHeight();

// Runs f(env, activity) with a JNIEnv on this thread, attaching only if it
// was not attached; pending Java exceptions are cleared.
template <class F>
void withJni(F&& f) {
  android_app* app = androidApp();
  if (!app || !app->activity) return;
  JavaVM* vm = app->activity->vm;
  JNIEnv* env = nullptr;
  const bool attached = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK;
  if (!attached && vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
  f(env, app->activity->clazz);
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!attached) vm->DetachCurrentThread();
}

// A key press that types a character (a hardware keyboard, or an IME that
// passes keys through) inserts it into the focused field.
void typeKey(int32_t keyCode, int32_t metaState);

}  // namespace glim::shell::detail
