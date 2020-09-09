#include <iostream>

#include <glim/paint/gl/Shader.h>
#include <glim/utils/Assert.h>

#include <glm/vec4.hpp>

namespace glim::paint::gl {
    Shader::Shader(const GLchar *source, Type type) noexcept {
        GLenum glType;
        switch (type) {
            case Type::Vertex:
                glType = GL_VERTEX_SHADER;
                break;
            case Type::Fragment:
                glType = GL_FRAGMENT_SHADER;
                break;
        }
        shader_ = glCreateShader(glType);
        GLIM_ASSERT_GL_ERROR();
        glShaderSource(*shader_, 1, &source, nullptr);
        glCompileShader(*shader_);
        GLint status;
        glGetShaderiv(*shader_, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            char buffer[512];
            glGetShaderInfoLog(*shader_, 512, nullptr, buffer);
            std::cerr << "Shader error: " << buffer << std::endl;
            std::abort();
        }
    }

    Shader::~Shader() {
        if (shader_) {
            glDeleteShader(*shader_);
        }
    }

    Program::Program(const Shader &vertex, const Shader &fragment) noexcept {
        program_ = glCreateProgram();
        GLIM_ASSERT_GL_ERROR();
        glAttachShader(*program_, *vertex.shader_);
        glAttachShader(*program_, *fragment.shader_);
        glLinkProgram(*program_);
        GLint success;
        glGetProgramiv(*program_, GL_LINK_STATUS, &success);
        if (!success) {
            GLchar buffer[512];
            glGetProgramInfoLog(*program_, 512, nullptr, buffer);
            std::cerr << "Program failed to link: " << buffer << std::endl;
            std::abort();
        }
    }

    Program &Program::operator=(Program &&other) noexcept {
        if (this != &other) {
            if (program_) {
                glDeleteProgram(*program_);
            }
            program_ = other.program_;
            other.program_ = std::nullopt;
        }
        return *this;
    }

    Program::~Program() {
        if (program_) {
            glDeleteProgram(*program_);
        }
    }

    void Program::use() {
        glUseProgram(*program_);
    }

    void Program::upload(const std::string &name, const Matrix &value) {
        auto location = glGetUniformLocation(*program_, name.c_str());
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(static_cast<const glm::mat4 &>(value)));
        GLIM_ASSERT_GL_ERROR();
    }

    void Program::upload(const std::string &name, const Color &value) {
        auto location = glGetUniformLocation(*program_, name.c_str());
        glm::vec4 color(value.red(), value.green(), value.blue(), value.alpha());
        glUniform4fv(location, 1, glm::value_ptr(color));
        GLIM_ASSERT_GL_ERROR();
    }
}
