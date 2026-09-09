#pragma once

#include <functional>
#include <string>

#include <glim/math.h>
#include <glim/shell/Event.h>

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
    void center();
    void show();
    void hide();
    void setEventCallback(EventCallback);

    Vec2 size() const;
    Vec2 drawableSize() const;
    float pixelRatio() const;
    void* nativeView() const;

    void dispatch(const Event&);

private:
    void* window_ = nullptr;
    void* view_ = nullptr;
    bool shown_ = false;
};

}  // namespace glim::shell
