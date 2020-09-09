#pragma once

#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace glim::paint {
    struct Point final {
        Point() = default;

        Point(float x, float y) noexcept
                : x_(x), y_(y) {}

        float x() const {
            return x_;
        }

        float y() const {
            return y_;
        }

    private:
        float x_ = 0;
        float y_ = 0;
    };

    struct Size final {
        Size() = default;

        Size(float width, float height) noexcept
                : width_(width), height_(height) {}

        float width() const {
            return width_;
        }

        float height() const {
            return height_;
        }

    private:
        float width_ = 0;
        float height_ = 0;
    };

    struct Radius final {
        Radius() = default;

        Radius(float radius) noexcept
                : leftTop_(radius), rightTop_(radius), leftBottom_(radius), rightBottom_(radius) {}

        Radius(float leftTop, float rightTop, float leftBottom, float rightBottom) noexcept
                : leftTop_(leftTop), rightTop_(rightTop), leftBottom_(leftBottom), rightBottom_(rightBottom) {}

        float leftTop() const {
            return leftTop_;
        }

        float rightTop() const {
            return rightTop_;
        }

        float leftBottom() const {
            return leftBottom_;
        }

        float rightBottom() const {
            return rightBottom_;
        }

    private:
        float leftTop_ = 0;
        float rightTop_ = 0;
        float leftBottom_ = 0;
        float rightBottom_ = 0;
    };

    struct Rectangle final {
        Rectangle() = default;

        Rectangle(const Point &origin, const Size &size) noexcept
                : origin_(origin), size_(size) {}

        Rectangle(const Size &size) noexcept
                : size_(size) {}

        const Point &origin() const {
            return origin_;
        }

        const Size &size() const {
            return size_;
        }

        float width() const {
            return size_.width();
        }

        float height() const {
            return size_.height();
        }

        float left() const {
            return origin_.x();
        }

        float top() const {
            return origin_.y();
        }

    private:
        Point origin_;
        Size size_;
    };

    struct Color final {
        Color() noexcept = default;

        Color(unsigned int rgba) noexcept: rgba_(rgba) {}

        Color(unsigned char r,
              unsigned char g,
              unsigned char b,
              unsigned char a = 0xff) noexcept {
            rgba_ = (r << 24) | (g << 16) | (b << 8) | a;
        }

        unsigned int rgba() const {
            return rgba_;
        }

        unsigned char r() const {
            return (rgba_ >> 24) & 0xff;
        }

        unsigned char g() const {
            return (rgba_ >> 16) & 0xff;
        }

        unsigned char b() const {
            return (rgba_ >> 8) & 0xff;
        }

        unsigned char a() const {
            return rgba_ & 0xff;
        }

        float red() const {
            return static_cast<float>(r()) / 255.0f;
        }

        float green() const {
            return static_cast<float>(g()) / 255.0f;
        }

        float blue() const {
            return static_cast<float>(b()) / 255.0f;
        }

        float alpha() const {
            return static_cast<float>(a()) / 255.0f;
        }

    private:
        unsigned int rgba_ = 0;
    };

    struct Matrix final {
        Matrix() noexcept = default;

        void setOrthogonal(float left, float top, float right, float bottom) {
            matrix_ = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
        }

        void setTranslate(float x, float y) {
            matrix_ = glm::translate(matrix_, glm::vec3(x, y, 0.0));
        }

        void setRotate(float degrees) {
            matrix_ = glm::rotate(matrix_, glm::radians(degrees), glm::vec3(0.0f, 0.0f, 1.0f));
        }

        void setScale(float x, float y) {
            matrix_ = glm::scale(matrix_, glm::vec3(x, y, 1.0));
        }

        explicit operator const glm::mat4 &() const {
            return matrix_;
        }

    private:
        glm::mat4 matrix_ = glm::mat4(1.0f);
    };
}
