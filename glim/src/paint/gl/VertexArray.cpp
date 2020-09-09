#include <OpenGL/gl3.h>

#include <glim/paint/gl/VertexArray.h>

namespace glim::paint::gl {
    VertexArray::VertexArray() noexcept {
        GLuint array;
        glGenVertexArrays(1, &array);
        array_ = array;
    }

    VertexArray::~VertexArray() {
        if (array_) {
            glDeleteVertexArrays(1, &*array_);
        }
    }

    void VertexArray::bind() {
        glBindVertexArray(*array_);
    }

    void VertexArray::unbind() {
        glBindVertexArray(0);
    }

    void VertexArray::addBuffer(Buffer buffer) {
        switch (buffer.type()) {
            case DataType::Float:
                glEnableVertexAttribArray(index_);
                glVertexAttribPointer(index_,
                                      buffer.count(),
                                      GL_FLOAT,
                                      GL_FALSE,
                                      buffer.count() * sizeof(GLfloat),
                                      nullptr);
                GLIM_ASSERT_GL_ERROR();
                index_++;
                break;
        }
        buffers_.push_back(std::move(buffer));
    }
}
