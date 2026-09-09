#pragma once

#include <optional>
#include <string>
#include <utility>

namespace glim::gpu {

enum class Status { Ok, Error };

template <typename T>
class Result {
public:
    static Result ok(T value) {
        Result r;
        r.status_ = Status::Ok;
        r.value_ = std::move(value);
        return r;
    }

    static Result fail(std::string message) {
        Result r;
        r.status_ = Status::Error;
        r.message_ = std::move(message);
        return r;
    }

    bool ok() const { return status_ == Status::Ok; }
    explicit operator bool() const { return ok(); }

    Status status() const { return status_; }
    const std::string& message() const { return message_; }

    T& value() { return *value_; }
    const T& value() const { return *value_; }

    T* operator->() { return &*value_; }
    const T* operator->() const { return &*value_; }

private:
    Status status_ = Status::Error;
    std::string message_;
    std::optional<T> value_;
};

}  // namespace glim::gpu
