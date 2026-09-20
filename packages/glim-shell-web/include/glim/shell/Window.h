#pragma once

#ifndef GLIM_SOFTWARE
#define GLIM_SOFTWARE 0
#endif

#include <functional>
#include <string>
#if GLIM_SOFTWARE
#include <cstdint>
#endif

#include <glim/math.h>
#include <glim/shell/Event.h>
#include <glim/shell/Slot.h>

namespace glim::shell {

// Web shell: browser canvas + WebGPU. Same API shape as macos/wayland shells.
// See docs/webgpu.md.
class Window final {
public:
    using EventCallback = std::function<void(const Event&)>;

    Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    ~Window();

    void setSize(int width, int height);
    void setPixelRatio(float ratio);
    void setTitle(const std::string&);
    void center();
    void show();
    void hide();
    void setEventCallback(EventCallback);
    void attachToScene(void* scene);

    Vec2 size() const;
    Vec2 drawableSize() const;
    float pixelRatio() const;
    Rect safeArea() const;
    void* nativeView() const;
    void attachSlot(std::uint32_t id, SlotNative);
    void positionSlot(std::uint32_t id, Rect windowLogical);
    void detachSlot(std::uint32_t id);
#if GLIM_SOFTWARE
    std::uint8_t* mapSoftware(int width, int height);
    void presentSoftware();
#endif

    void dispatch(const Event&);

    // Web-only: canvas selector (default "#glim"). Set before show().
    void setCanvasSelector(const std::string& selector);
    const std::string& canvasSelector() const { return canvasSelector_; }

private:
    std::string title_ = "Glim";
    std::string canvasSelector_ = "#glim";
    int width_ = 720;
    int height_ = 480;
    float pixelRatio_ = 1.0f;
    bool shown_ = false;
    EventCallback callback_;
    SlotTable slots_{};
};

}  // namespace glim::shell
