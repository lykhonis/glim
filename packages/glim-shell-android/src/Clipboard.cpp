#include <glim/shell/Clipboard.h>

#include <string>

#include "App.h"

namespace glim::shell {

ClipboardText readClipboard() {
  ClipboardText out;
  detail::withJni([&](JNIEnv* env, jobject activity) {
    jclass cls = env->GetObjectClass(activity);
    jmethodID read = env->GetMethodID(cls, "readClipboard", "()Ljava/lang/String;");
    if (!read) return;
    auto text = static_cast<jstring>(env->CallObjectMethod(activity, read));
    if (env->ExceptionCheck()) return;
    out.status = ClipboardStatus::Empty;
    if (!text) return;
    const char* c = env->GetStringUTFChars(text, nullptr);
    out.text = c ? c : "";
    env->ReleaseStringUTFChars(text, c);
    env->DeleteLocalRef(text);
    if (!out.text.empty()) out.status = ClipboardStatus::Ok;
  });
  return out;
}

ClipboardStatus writeClipboard(std::string_view text) {
  ClipboardStatus status = ClipboardStatus::Unsupported;
  detail::withJni([&](JNIEnv* env, jobject activity) {
    jclass cls = env->GetObjectClass(activity);
    jmethodID write = env->GetMethodID(cls, "writeClipboard", "(Ljava/lang/String;)V");
    if (!write) return;
    jstring value = env->NewStringUTF(std::string(text).c_str());
    env->CallVoidMethod(activity, write, value);
    env->DeleteLocalRef(value);
    if (!env->ExceptionCheck()) status = ClipboardStatus::Ok;
  });
  return status;
}

}  // namespace glim::shell
