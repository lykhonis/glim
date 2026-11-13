#pragma once

#include <coroutine>
#include <exception>

namespace glim::transport {

// Fire-and-forget coroutine: starts at once and frees itself when done.
struct Task {
  struct promise_type {
    Task get_return_object() noexcept { return {}; }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept { std::terminate(); }
  };
};

}  // namespace glim::transport
