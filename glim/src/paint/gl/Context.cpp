#include <glim/paint/gl/Context.h>

#include <glm/gtc/matrix_transform.hpp>

namespace glim::paint {
    void Context::setSize(const Size &size) {
        state_.projection->setOrthogonal(0.0f, 0, size.width(), size.height());
    }

    void Context::translate(const Point &offset) {
        state_.model->setTranslate(offset.x(), offset.y());
    }

    void Context::rotate(float degrees, const Point &anchor) {
        state_.model->setTranslate(anchor.x(), anchor.y());
        state_.model->setRotate(degrees);
        state_.model->setTranslate(-anchor.x(), -anchor.y());
    }

    void Context::scale(const Size &size) {
        state_.model->setScale(size.width(), size.height());
    }

    void Context::save() {
        states_.push_back(state_);
    }

    void Context::restore() {
        if (!states_.empty()) {
            state_ = states_.back();
            states_.pop_back();
        }
    }

    void Context::clear(const Rectangle &rectangle) {
        save();
        setFillColor(0);
        fill(rectangle);
        restore();
    }

    void Context::fill(const Rectangle &rectangle) {
        save();
        translate(rectangle.origin());
        scale(rectangle.size());
        bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        GLIM_ASSERT_GL_ERROR();
        restore();
    }

    void Context::setFillColor(const Color &color) {
        state_.fillColor = color;
    }

    void Context::bind() {
        if (!program_) {
            program_ = gl::Program(
                    gl::Shader(
                            GLSL(330,
                                 layout(location = 0) in vec2 position;
                                         uniform mat4 model;
                                         uniform mat4 projection;
                                         void main() {
                                             gl_Position = projection * model * vec4(position, 0.0, 1.0);
                                         }),
                            gl::Shader::Type::Vertex),
                    gl::Shader(
                            GLSL(330,
                                 layout(location = 0) out vec4 outColor;
                                         uniform vec4 color;
                                         void main() {
                                             outColor = color;
                                         }),
                            gl::Shader::Type::Fragment));
        }
        program_->use();
        if (state_.projection.use()) {
            program_->upload("projection", state_.projection);
        }
        if (state_.model.use()) {
            program_->upload("model", state_.model);
        }
        if (state_.fillColor.use()) {
            program_->upload("color", state_.fillColor);
        }
        if (!vertexArray_) {
            GLfloat vertices[] = {
                    0.0f, 1.0f,
                    1.0f, 0.0f,
                    0.0f, 0.0f,
                    0.0f, 1.0f,
                    1.0f, 1.0f,
                    1.0f, 0.0f,
            };
            auto buffer = gl::Buffer(2, vertices, sizeof(vertices));
            vertexArray_ = gl::VertexArray();
            vertexArray_->bind();
            vertexArray_->addBuffer(std::move(buffer));
        } else {
            vertexArray_->bind();
        }
    }
}
