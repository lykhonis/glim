#pragma once

#include <vector>

#include <glim/paint/Primitives.h>
#include <glim/paint/gl/Shader.h>
#include <glim/paint/gl/VertexArray.h>

namespace glim::paint {
    class Context final {
    public:
        void setSize(const Size &);

        void clear(const Rectangle &);

        void translate(const Point &);

        void scale(const Size &);

        void rotate(float degrees, const Point &anchor);

        void setFillColor(const Color &);

        void fill(const Rectangle &);

        void save();

        void restore();

    private:
        template<typename T>
        struct Resource {
            Resource &operator=(const T &data) {
                data_ = data;
                modified_ = true;
                return *this;
            }

            Resource &operator=(T &&data) {
                data_ = std::move(data);
                modified_ = true;
                return *this;
            }

            bool use() {
                if (modified_) {
                    modified_ = false;
                    return true;
                } else {
                    return false;
                }
            }

            T *operator->() {
                modified_ = true;
                return &data_;
            }

            operator const T &() const {
                return data_;
            }

            operator T &() {
                return data_;
            }

        private:
            T data_;
            bool modified_ = false;
        };

        struct State {
            Resource<Matrix> model;
            Resource<Matrix> projection;
            Resource<Color> fillColor;
        };

        void bind();

        std::optional<gl::Program> program_;
        std::optional<gl::VertexArray> vertexArray_;
        State state_;
        std::vector<State> states_;
    };
}
