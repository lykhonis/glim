#pragma once

namespace glim::shell::macos {
    class Event final {
    public:
        enum class Type {
            None,
            WindowClosed,
        };

        explicit Event(Type type)
                : type_(type) {}

        Type type() const {
            return type_;
        }

        explicit operator bool() const {
            return type_ != Type::None;
        }

    private:
        Type type_ = Type::None;
    };
}
