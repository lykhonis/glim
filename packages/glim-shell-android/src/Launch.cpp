#include "Launch.h"

#include <android/log.h>
#include <jni.h>
#include <unistd.h>

#include <cstdio>
#include <sstream>
#include <thread>

namespace glim::shell::detail {

std::vector<std::string> launchArgs(android_app* app) {
  std::vector<std::string> args{"glim"};
  JNIEnv* env = nullptr;
  JavaVM* vm = app->activity->vm;
  if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return args;
  jobject activity = app->activity->clazz;
  jclass activityClass = env->GetObjectClass(activity);
  jmethodID getIntent = env->GetMethodID(activityClass, "getIntent", "()Landroid/content/Intent;");
  jobject intent = env->CallObjectMethod(activity, getIntent);
  if (intent != nullptr) {
    jclass intentClass = env->GetObjectClass(intent);
    jmethodID getExtra =
      env->GetMethodID(intentClass, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
    jstring key = env->NewStringUTF("args");
    auto value = static_cast<jstring>(env->CallObjectMethod(intent, getExtra, key));
    if (value != nullptr) {
      const char* chars = env->GetStringUTFChars(value, nullptr);
      std::istringstream words(chars);
      for (std::string w; words >> w;) args.push_back(w);
      env->ReleaseStringUTFChars(value, chars);
    }
  }
  if (env->ExceptionCheck()) env->ExceptionClear();
  vm->DetachCurrentThread();
  return args;
}

void forwardStdioToLogcat() {
  static bool started = false;
  if (started) return;
  int fds[2];
  if (pipe(fds) != 0) return;
  started = true;
  setvbuf(stdout, nullptr, _IOLBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  dup2(fds[1], STDOUT_FILENO);
  dup2(fds[1], STDERR_FILENO);
  close(fds[1]);
  std::thread([fd = fds[0]] {
    std::string line;
    char buf[512];
    for (ssize_t n; (n = read(fd, buf, sizeof buf)) > 0;) {
      for (ssize_t i = 0; i < n; ++i) {
        if (buf[i] != '\n') {
          line += buf[i];
          continue;
        }
        __android_log_write(ANDROID_LOG_INFO, "glim", line.c_str());
        line.clear();
      }
    }
  }).detach();
}

}  // namespace glim::shell::detail
