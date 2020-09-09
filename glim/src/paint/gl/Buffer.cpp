#include <glim/paint/gl/Buffer.h>

namespace glim::paint::gl {
    Buffer::Buffer(int count, const float *data, int size) noexcept
            : count_(count), type_(DataType::Float) {
        GLuint buffer;
        glGenBuffers(1, &buffer);
        buffer_ = buffer;
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
        GLIM_ASSERT_GL_ERROR();
    }

    Buffer &Buffer::operator=(Buffer &&other) noexcept {
        if (this != &other) {
            if (buffer_) {
                glDeleteBuffers(1, &*buffer_);
            }
            buffer_ = other.buffer_;
            count_ = other.count_;
            type_ = other.type_;
            other.buffer_ = std::nullopt;
        }
        return *this;
    }

    Buffer::~Buffer() {
        if (buffer_) {
            glDeleteBuffers(1, &*buffer_);
        }
    }

    void Buffer::bind() {
        glBindBuffer(GL_ARRAY_BUFFER, *buffer_);
    }

    void Buffer::unbind() {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}
