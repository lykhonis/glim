#include "App.h"

#include <android/configuration.h>

#include <algorithm>

namespace glim::shell::detail {
namespace {

android_app* gApp = nullptr;

}  // namespace

android_app* androidApp() {
    return gApp;
}

void setAndroidApp(android_app* app) {
    gApp = app;
}

ANativeWindow* nativeWindow() {
    return gApp ? gApp->window : nullptr;
}

float pixelRatio() {
    if (!gApp || !gApp->config) {
        return 1.0f;
    }
    const int32_t dpi = AConfiguration_getDensity(gApp->config);
    if (dpi <= 0 || dpi == ACONFIGURATION_DENSITY_NONE || dpi == ACONFIGURATION_DENSITY_DEFAULT) {
        return 1.0f;
    }
    return std::max(1.0f, static_cast<float>(dpi) / 160.0f);
}

int drawableWidth() {
    ANativeWindow* window = nativeWindow();
    return window ? std::max(1, ANativeWindow_getWidth(window)) : 1;
}

int drawableHeight() {
    ANativeWindow* window = nativeWindow();
    return window ? std::max(1, ANativeWindow_getHeight(window)) : 1;
}

}  // namespace glim::shell::detail
