#include <android/hardware_buffer.h>
#include <android/input.h>
#include <android/native_window_jni.h>
#include <glim/shell/AndroidActivity.h>
#include <media/NdkImageReader.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>

#include <cmath>
#include <deque>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "PageHtml.h"
#include "WebPage.h"
#include "glim/toolkit/host/Feed.h"

namespace glim::toolkit {

namespace {

// ART aborts when an attached native thread exits, so detach on exit.
JNIEnv* jniEnv(JavaVM* vm) {
  static pthread_key_t key;
  static pthread_once_t once = PTHREAD_ONCE_INIT;
  pthread_once(&once, [] {
    pthread_key_create(&key, [](void* v) {
      static_cast<JavaVM*>(v)->DetachCurrentThread();
    });
  });
  JNIEnv* env = nullptr;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) ==
      JNI_OK)
    return env;
  if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
  pthread_setspecific(key, vm);
  return env;
}

bool failed(JNIEnv* env, const char* what) {
  if (!env->ExceptionCheck()) return false;
  env->ExceptionDescribe();
  env->ExceptionClear();
  std::cerr << "webview: " << what << " threw\n";
  return true;
}

// Native threads see only the boot class loader; app classes come from
// the activity's.
jclass appClass(JNIEnv* env, jobject activity, const char* name) {
  jclass activityClass = env->GetObjectClass(activity);
  jmethodID getLoader = env->GetMethodID(
    activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;"
  );
  jobject loader = env->CallObjectMethod(activity, getLoader);
  jclass loaderClass = env->FindClass("java/lang/ClassLoader");
  jmethodID load = env->GetMethodID(
    loaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"
  );
  auto found = static_cast<jclass>(
    env->CallObjectMethod(loader, load, env->NewStringUTF(name))
  );
  return failed(env, name) ? nullptr : found;
}

int motionAction(LayerInput::Phase phase) {
  switch (phase) {
    case LayerInput::Phase::Down:
      return AMOTION_EVENT_ACTION_DOWN;
    case LayerInput::Phase::Move:
      return AMOTION_EVENT_ACTION_MOVE;
    case LayerInput::Phase::Up:
      return AMOTION_EVENT_ACTION_UP;
    case LayerInput::Phase::Wheel:
      break;
  }
  return -1;
}

// An image the frame may sample: open until Feed::claim frees it, so
// the reader cannot hand its buffer back to the page while the GPU
// reads it.
struct Held {
  AImage* image = nullptr;
  std::uint32_t id = 0;
  int fence = -1;
  std::uint64_t seq = 0;
};

// Texture (K86): WebView → private VirtualDisplay → AImageReader of
// GPU-sampled AHardwareBuffers → imported and sampled with no copy.
class AndroidWebPage final : public WebPage {
 public:
  AndroidWebPage() : feed_([this] { return signaled(); }) {}

  ~AndroidWebPage() override {
    for (Held& h : held_) release(h);
    if (reader_) AImageReader_delete(reader_);
  }

  void attach(paint::Context& ctx) override {
    if (reader_ || broken_ || ctx.size().x <= 0) return;
    if (__builtin_available(android 26, *)) {
      broken_ = !open(ctx);
    } else {
      broken_ = true;
    }
  }

  bool advance() override {
    if (!reader_) return false;
    if (__builtin_available(android 26, *)) {
      reclaim();
      take();
    }
    const bool statusMoved = pollStatus();
    return published_ > shown_ || statusMoved;
  }

  WebStatus status() const override { return status_; }

  void load(const std::string& url) override {
    call(load_, url.c_str(), "load");
  }

  void reload() override { call(reload_, nullptr, "reload"); }

  std::uint32_t acquire() override {
    const std::uint32_t id = feed_.acquire();
    for (const Held& h : held_) {
      if (h.id == id) shown_ = h.seq;
    }
    return id;
  }

  bool input(const LayerInput& in) override {
    const int action = motionAction(in.phase);
    if (!page_ || action < 0 || in.width <= 0 || in.height <= 0)
      return false;
    JNIEnv* env = jniEnv(vm_);
    if (!env) return false;
    const float x = in.x * static_cast<float>(width_) / in.width;
    const float y = in.y * static_cast<float>(height_) / in.height;
    env->CallVoidMethod(page_, touch_, action, x, y);
    return !failed(env, "touch");
  }

  const char* mode() const override {
    return "Texture: WebView in a VirtualDisplay";
  }

 private:
  // One int over JNI per frame; strings only when the page changed.
  bool pollStatus() {
    JNIEnv* env = page_ ? jniEnv(vm_) : nullptr;
    if (!env) return false;
    const jint version = env->CallIntMethod(page_, version_);
    if (failed(env, "version") || version == version_seen_)
      return false;
    version_seen_ = version;
    const auto text = [&](jmethodID m) {
      auto s = static_cast<jstring>(env->CallObjectMethod(page_, m));
      if (failed(env, "status") || !s) return std::string();
      const char* c = env->GetStringUTFChars(s, nullptr);
      std::string out(c ? c : "");
      env->ReleaseStringUTFChars(s, c);
      env->DeleteLocalRef(s);
      return out;
    };
    WebStatus next{
      .url = text(url_),
      .title = text(title_),
      .progress =
        static_cast<float>(env->CallIntMethod(page_, progress_)) /
        100.0f,
      .loading = env->CallBooleanMethod(page_, loading_) == JNI_TRUE,
    };
    if (failed(env, "status") || next == status_) return false;
    status_ = std::move(next);
    if (!status_.loading) {
      std::cerr << "webview status " << status_.url << " \""
                << status_.title << "\"" << std::endl;
    }
    return true;
  }

  void call(jmethodID method, const char* arg, const char* what) {
    JNIEnv* env = page_ ? jniEnv(vm_) : nullptr;
    if (!env) return;
    if (arg) {
      jstring s = env->NewStringUTF(arg);
      env->CallVoidMethod(page_, method, s);
      env->DeleteLocalRef(s);
    } else {
      env->CallVoidMethod(page_, method);
    }
    failed(env, what);
  }

  static constexpr std::size_t kHeldMax = host::Feed::kHeld + 2;
  // One more than held, so the page always has a buffer to draw into.
  static constexpr int kImages = static_cast<int>(kHeldMax) + 1;

  __attribute__((availability(android, introduced = 26))) bool open(
    paint::Context& ctx
  ) {
    const shell::AndroidActivity host = shell::androidActivity();
    vm_ = host.vm;
    JNIEnv* env = vm_ ? jniEnv(vm_) : nullptr;
    if (!env) return false;
    jclass cls =
      appClass(env, host.activity, "com.glim.webview.WebTexture");
    if (!cls) return false;
    const jint dpi = env->CallStaticIntMethod(
      cls,
      env->GetStaticMethodID(
        cls, "densityDpi", "(Landroid/app/Activity;)I"
      ),
      host.activity
    );
    if (failed(env, "densityDpi") || dpi <= 0) return false;
    const float scale = static_cast<float>(dpi) / 160.0f;
    width_ = static_cast<int>(std::lround(ctx.size().x * scale));
    height_ = static_cast<int>(std::lround(ctx.size().y * scale));
    constexpr std::uint64_t usage =
      AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
      AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
    if (AImageReader_newWithUsage(
          width_,
          height_,
          AIMAGE_FORMAT_RGBA_8888,
          usage,
          kImages,
          &reader_
        ) != AMEDIA_OK) {
      reader_ = nullptr;
      return false;
    }
    ANativeWindow* window = nullptr;
    AImageReader_getWindow(reader_, &window);
    jobject surface = ANativeWindow_toSurface(env, window);
    jmethodID make = env->GetMethodID(
      cls,
      "<init>",
      "(Landroid/app/Activity;Landroid/view/Surface;IIILjava/lang/"
      "String;)V"
    );
    jobject page = env->NewObject(
      cls,
      make,
      host.activity,
      surface,
      width_,
      height_,
      dpi,
      env->NewStringUTF(kPageHtml)
    );
    if (failed(env, "WebTexture") || !page) return false;
    page_ = env->NewGlobalRef(page);
    touch_ = env->GetMethodID(cls, "touch", "(IFF)V");
    load_ = env->GetMethodID(cls, "load", "(Ljava/lang/String;)V");
    reload_ = env->GetMethodID(cls, "reload", "()V");
    version_ = env->GetMethodID(cls, "version", "()I");
    url_ = env->GetMethodID(cls, "url", "()Ljava/lang/String;");
    title_ = env->GetMethodID(cls, "title", "()Ljava/lang/String;");
    progress_ = env->GetMethodID(cls, "progress", "()I");
    loading_ = env->GetMethodID(cls, "loading", "()Z");
    if (failed(env, "methods")) return false;
    ctx_ = &ctx;
    std::cerr << "webview texture " << width_ << "x" << height_
              << " dpi " << dpi << std::endl;
    return true;
  }

  __attribute__((availability(android, introduced = 26))) void reclaim(
  ) {
    while (held_.size() >= kHeldMax) {
      std::vector<std::uint32_t> ids;
      for (const Held& h : held_) ids.push_back(h.id);
      const std::optional<std::size_t> free = feed_.claim(ids);
      if (!free) return;
      release(held_[*free]);
      held_.erase(held_.begin() + static_cast<std::ptrdiff_t>(*free));
    }
  }

  __attribute__((availability(android, introduced = 26))) void take() {
    if (held_.size() >= kHeldMax) return;
    Held h;
    if (AImageReader_acquireLatestImageAsync(
          reader_, &h.image, &h.fence
        ) != AMEDIA_OK)
      return;
    AHardwareBuffer* buffer = nullptr;
    if (AImage_getHardwareBuffer(h.image, &buffer) != AMEDIA_OK ||
        !buffer) {
      release(h);
      return;
    }
    std::uint32_t& id = ids_[buffer];
    if (id == 0)
      id = ctx_->wrapSurface(
        buffer, width_, height_, paint::SampleFormat::Rgba8Unorm
      );
    h.id = id;
    h.seq = ++published_;
    held_.push_back(h);
    feed_.publish({h.id, h.seq});
    if (published_ % 30 == 1) {
      const host::FeedStats st = feed_.stats();
      std::cerr << "webview feed published=" << st.published
                << " acquired=" << st.acquired
                << " fence_misses=" << st.fenceMisses
                << " dropped=" << st.dropped
                << " reuse_waits=" << st.reuseWaits << std::endl;
    }
  }

  // The newest sequence whose reader fence, and every older one, has
  // passed.
  std::uint64_t signaled() {
    for (Held& h : held_) {
      if (h.fence >= 0) {
        pollfd p{h.fence, POLLIN, 0};
        if (poll(&p, 1, 0) <= 0) return h.seq - 1;
        close(h.fence);
        h.fence = -1;
      }
    }
    return published_;
  }

  static void release(Held& h) {
    if (h.fence >= 0) close(h.fence);
    if (h.image) {
      if (__builtin_available(android 26, *)) AImage_delete(h.image);
    }
    h = {};
  }

  host::Feed feed_;
  AImageReader* reader_ = nullptr;
  paint::Context* ctx_ = nullptr;
  JavaVM* vm_ = nullptr;
  jobject page_ = nullptr;
  jmethodID touch_ = nullptr;
  jmethodID load_ = nullptr;
  jmethodID reload_ = nullptr;
  jmethodID version_ = nullptr;
  jmethodID url_ = nullptr;
  jmethodID title_ = nullptr;
  jmethodID progress_ = nullptr;
  jmethodID loading_ = nullptr;
  jint version_seen_ = -1;
  WebStatus status_;
  int width_ = 0;
  int height_ = 0;
  bool broken_ = false;
  std::deque<Held> held_;
  std::unordered_map<AHardwareBuffer*, std::uint32_t> ids_;
  std::uint64_t published_ = 0;
  std::uint64_t shown_ = 0;
};

}  // namespace

std::unique_ptr<WebPage> makeWebPage() {
  return std::make_unique<AndroidWebPage>();
}

}  // namespace glim::toolkit
