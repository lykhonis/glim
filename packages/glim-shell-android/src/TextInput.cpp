#include <glim/shell/ImeEdits.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Utf16.h>
#include <glim/shell/Window.h>
#include <jni.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "App.h"

namespace glim::shell {

namespace {

// The IME's calls arrive on the UI thread and run on the native thread,
// where the field state and callback live.
struct Ime {
  TextInputState state;
  TextInputCallback callback;
  std::optional<RunLoop> loop;
  bool registered = false;
};

Ime& ime() {
  static Ime i;
  return i;
}

using Edit = std::function<std::vector<TextInputChange>(const TextInputState&)>;

void post(Edit edit) {
  Ime& i = ime();
  if (!i.loop) return;
  i.loop->scheduleTask([edit = std::move(edit)](RunLoop::TaskContext&) {
    Ime& in = ime();
    if (!in.state.active || !in.callback) return;
    for (const TextInputChange& c : edit(in.state)) in.callback(c);
  });
}

std::string utf8(JNIEnv* env, jstring s) {
  const char* c = env->GetStringUTFChars(s, nullptr);
  std::string out = c ? c : "";
  env->ReleaseStringUTFChars(s, c);
  return out;
}

void compose(JNIEnv* env, jclass, jstring text) {
  post([t = utf8(env, text)](const TextInputState& s) { return imeCompose(s, t); });
}

void commit(JNIEnv* env, jclass, jstring text) {
  post([t = utf8(env, text)](const TextInputState& s) { return imeCommit(s, t); });
}

void finishComposing(JNIEnv*, jclass) {
  post([](const TextInputState& s) {
    return s.marked ? std::vector<TextInputChange>{{.op = TextInputOp::Unmark}}
                    : std::vector<TextInputChange>{};
  });
}

void select(JNIEnv*, jclass, jint start, jint end) {
  post([start, end](const TextInputState& s) {
    return imeSelect(s, static_cast<std::size_t>(start), static_cast<std::size_t>(end));
  });
}

void deleteSurrounding(JNIEnv*, jclass, jint before, jint after) {
  post([before, after](const TextInputState& s) {
    return imeDeleteSurrounding(
      s, static_cast<std::size_t>(before), static_cast<std::size_t>(after)
    );
  });
}

void deleteBackward(JNIEnv*, jclass) {
  post([](const TextInputState&) {
    return std::vector<TextInputChange>{{.op = TextInputOp::DeleteBackward}};
  });
}

void deleteForward(JNIEnv*, jclass) {
  post([](const TextInputState&) {
    return std::vector<TextInputChange>{{.op = TextInputOp::DeleteForward}};
  });
}

// App classes are not visible to FindClass off the UI thread; the
// activity's class loader finds them.
void registerNatives(JNIEnv* env, jobject activity) {
  jclass activityClass = env->GetObjectClass(activity);
  jmethodID getLoader =
    env->GetMethodID(activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
  jobject loader = env->CallObjectMethod(activity, getLoader);
  jclass loaderClass = env->GetObjectClass(loader);
  jmethodID load =
    env->GetMethodID(loaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
  jstring name = env->NewStringUTF("com.glim.shell.TextInputView");
  auto view = static_cast<jclass>(env->CallObjectMethod(loader, load, name));
  if (!view) return;
  const JNINativeMethod methods[] = {
    {"nativeCompose", "(Ljava/lang/String;)V", reinterpret_cast<void*>(compose)},
    {"nativeCommit", "(Ljava/lang/String;)V", reinterpret_cast<void*>(commit)},
    {"nativeFinishComposing", "()V", reinterpret_cast<void*>(finishComposing)},
    {"nativeSelect", "(II)V", reinterpret_cast<void*>(select)},
    {"nativeDeleteSurrounding", "(II)V", reinterpret_cast<void*>(deleteSurrounding)},
    {"nativeDeleteBackward", "()V", reinterpret_cast<void*>(deleteBackward)},
    {"nativeDeleteForward", "()V", reinterpret_cast<void*>(deleteForward)},
  };
  env->RegisterNatives(view, methods, sizeof(methods) / sizeof(methods[0]));
}

// Java strings are UTF-16, so the field crosses with UTF-16 positions.
void publishToJava(JNIEnv* env, jobject activity, const TextInputState& s) {
  jclass activityClass = env->GetObjectClass(activity);
  jmethodID publish = env->GetMethodID(activityClass, "publishText", "(ZZLjava/lang/String;II)V");
  if (!publish) return;
  jstring value = env->NewStringUTF(s.text.c_str());
  const auto a = static_cast<jint>(utf16Units(s.text, s.selection.start));
  const auto b = static_cast<jint>(utf16Units(s.text, s.selection.start + s.selection.length));
  env->CallVoidMethod(activity, publish, s.active, s.secure, value, a, b);
  env->DeleteLocalRef(value);
}

}  // namespace

void Window::setTextInput(TextInputState state) {
  Ime& i = ime();
  const bool changed = state.active != i.state.active || state.text != i.state.text ||
                       state.selection.start != i.state.selection.start ||
                       state.selection.length != i.state.selection.length ||
                       state.secure != i.state.secure;
  i.state = std::move(state);
  if (!changed) return;
  detail::withJni([&](JNIEnv* env, jobject activity) {
    if (!i.registered) {
      registerNatives(env, activity);
      i.registered = true;
    }
    publishToJava(env, activity, i.state);
  });
}

void Window::setTextInputCallback(TextInputCallback callback) {
  Ime& i = ime();
  i.callback = std::move(callback);
  if (!i.loop) i.loop.emplace();
}

}  // namespace glim::shell
