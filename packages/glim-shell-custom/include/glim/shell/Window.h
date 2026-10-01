#pragma once

#ifndef GLIM_SOFTWARE
#define GLIM_SOFTWARE 0
#endif

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glim/math.h>
#include <glim/shell/Event.h>
#include <glim/shell/Slot.h>

namespace glim::shell {

// Custom shell: embedder-owned surface (QNX / Integrity / TV bring-up).
// Headless stub with zero OS dependencies. The OEM owns the frame clock and
// drives frames via inject() / injectFrame(); Application::run() returns
// immediately on the host. Same Window API shape as the other shells.
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
    std::size_t slotCount() const { return slotRects_.size(); }
    template <typename Fn>
    void forEachSlot(Fn&& fn) const {
        for (const auto& e : slotRects_) {
            fn(e.first, e.second);
        }
    }
#if GLIM_SOFTWARE
    std::uint8_t* mapSoftware(int width, int height);
    void presentSoftware();
#endif

    void dispatch(const Event&);

    void inject(const Event& event) { dispatch(event); }
    void injectFrame();

private:
    std::string title_ = "Glim";
    int width_ = 720;
    int height_ = 480;
    float pixelRatio_ = 1.0f;
    bool shown_ = false;
    EventCallback callback_;
    SlotTable slots_{};
    std::unordered_map<std::uint32_t, Rect> slotRects_{};
    // Unconditional storage: only Window.cpp is built with GLIM_SOFTWARE=1,
    // so guarded members would change the class layout seen by consumers.
    std::vector<std::uint8_t> software_;
    int softwareW_ = 0;
    int softwareH_ = 0;
};

}  // namespace glim::shell
