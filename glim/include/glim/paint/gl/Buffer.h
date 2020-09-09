#pragma once

#include <optional>

#include <OpenGL/gl.h>

#include <glim/utils/Assert.h>

namespace glim::paint::gl {
    enum class DataType {
        Float,
    };

    class Buffer final {
    public:
        Buffer(int count, const float *data, int size) noexcept;

        Buffer(const Buffer &) = delete;

        Buffer(Buffer &&other) noexcept
                : buffer_(other.buffer_), count_(other.count_), type_(other.type_) {
            other.buffer_ = std::nullopt;
        }

        Buffer &operator=(const Buffer &) = delete;

        Buffer &operator=(Buffer &&) noexcept;

        ~Buffer();

        DataType type() const {
            return type_;
        }

        int count() const {
            return count_;
        }

        void bind();

        void unbind();

    private:
        std::optional<GLuint> buffer_;
        int count_;
        DataType type_;
    };
}
