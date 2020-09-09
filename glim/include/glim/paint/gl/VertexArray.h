#pragma once

#include <vector>

#include <glim/paint/gl/Buffer.h>

namespace glim::paint::gl {
    class VertexArray final {
    public:
        VertexArray() noexcept;

        VertexArray(const VertexArray &) = delete;

        VertexArray(VertexArray &&other) noexcept
                : index_(other.index_), array_(other.array_), buffers_(std::move(other.buffers_)) {
            other.array_ = std::nullopt;
        }

        VertexArray &operator=(const VertexArray &) = delete;

        VertexArray &operator=(VertexArray &&other) noexcept {
            if (this != &other) {
                index_ = other.index_;
                array_ = other.array_;
                buffers_ = std::move(other.buffers_);
                other.array_ = std::nullopt;
            }
            return *this;
        }

        ~VertexArray();

        void bind();

        void unbind();

        void addBuffer(Buffer);

    private:
        int index_ = 0;
        std::optional<GLuint> array_;
        std::vector<Buffer> buffers_;
    };
}
