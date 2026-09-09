#pragma once

namespace glim::shell {

enum class EventType {
    None,
    WindowClosed,
    WindowResized,
    Frame,
    KeyDown,
    KeyUp,
    PointerDown,
    PointerUp,
    PointerMove,
};

enum class Key {
    Unknown,
    Enter,
    Escape,
    Back,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    DpadCenter,
};

enum class PointerButton {
    None,
    Left,
    Right,
    Middle,
};

class Event {
public:
    Event() = default;
    explicit Event(EventType type) : type_(type) {}

    EventType type() const { return type_; }
    int width() const { return width_; }
    int height() const { return height_; }
    float x() const { return x_; }
    float y() const { return y_; }
    Key key() const { return key_; }
    PointerButton button() const { return button_; }
    int pointerId() const { return pointerId_; }

    Event& setSize(int w, int h) {
        width_ = w;
        height_ = h;
        return *this;
    }

    Event& setPoint(float x, float y) {
        x_ = x;
        y_ = y;
        return *this;
    }

    Event& setKey(Key key) {
        key_ = key;
        return *this;
    }

    Event& setButton(PointerButton button) {
        button_ = button;
        return *this;
    }

    Event& setPointerId(int id) {
        pointerId_ = id;
        return *this;
    }

    explicit operator bool() const { return type_ != EventType::None; }

private:
    EventType type_ = EventType::None;
    int width_ = 0;
    int height_ = 0;
    float x_ = 0;
    float y_ = 0;
    Key key_ = Key::Unknown;
    PointerButton button_ = PointerButton::None;
    int pointerId_ = 0;
};

}  // namespace glim::shell
