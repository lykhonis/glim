#pragma once

#include <optional>
#include <string>

#include <OpenGL/gl.h>

#include <glim/paint/Primitives.h>

#define GLSL(version, shader) "#version " #version " core\n" #shader

namespace glim::paint::gl {
    class Program;

    class Shader final {
    public:
        enum class Type {
            Vertex,
            Fragment,
        };

        Shader(const GLchar *source, Type type) noexcept;

        Shader(const Shader &) = delete;

        Shader(Shader &&other) noexcept: shader_(other.shader_) {
            other.shader_ = std::nullopt;
        }

        ~Shader();

        Shader &operator=(const Shader &) = delete;

        Shader &operator=(Shader &&other) noexcept {
            if (this != &other) {
                shader_ = other.shader_;
                other.shader_ = std::nullopt;
            }
            return *this;
        }

    private:
        friend class Program;

        std::optional<GLuint> shader_;
    };

    class Program final {
    public:
        Program(const Shader &vertex, const Shader &fragment) noexcept;

        Program(const Program &) = delete;

        Program(Program &&other) noexcept: program_(other.program_) {
            other.program_ = std::nullopt;
        }

        ~Program();

        Program &operator=(const Program &) = delete;

        Program &operator=(Program &&) noexcept;

        void use();

        void upload(const std::string &name, const Matrix &);

        void upload(const std::string &name, const Color &);

    private:
        std::optional<GLuint> program_;
    };
}
