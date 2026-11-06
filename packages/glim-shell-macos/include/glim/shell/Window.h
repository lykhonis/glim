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

class Window final {
 public:
  using EventCallback = std::function<void(const Event&)>;

  Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  ~Window();

  void setSize(int width, int height);
  void setTitle(const std::string&);
  // Modern composable chrome (macOS): hides the system title text,
  // makes the titlebar transparent, and extends content under it so
  // the app paints its own bar. Traffic lights stay system-owned.
  void setCustomTitlebar(bool enabled);
  // Begin a native window drag using the current NSEvent. Call from
  // a PointerDown on an empty header area that the toolkit did not
  // consume — empty chrome drags, controls still click.
  void dragWindow();
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

 private:
  void* window_ = nullptr;
  void* view_ = nullptr;
  bool shown_ = false;
  void* software_ = nullptr;
  SlotTable slots_{};
};

}  // namespace glim::shell
