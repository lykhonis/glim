#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>

namespace glim {
namespace toolkit {

template <class T>
struct Cell {
  T value;
  std::uint32_t id;
  Cell(T v, std::uint32_t i) : value(std::move(v)), id(i) {}
};

template <class T>
class State {
 public:
  State() = default;
  explicit State(std::shared_ptr<Cell<T>> p) : cell_(std::move(p)) {}
  explicit operator bool() const { return static_cast<bool>(cell_); }
  bool operator==(const State& o) const { return cell_ == o.cell_; }
  bool operator!=(const State& o) const { return cell_ != o.cell_; }
  const T& get() const {
    assert(cell_ && "State handle is empty (default-constructed or moved-from)");
    return cell_->value;
  }
  void set(const T& v) const {
    assert(cell_ && "State handle is empty (default-constructed or moved-from)");
    cell_->value = v;
  }
  void set(T&& v) const {
    assert(cell_ && "State handle is empty (default-constructed or moved-from)");
    cell_->value = std::move(v);
  }
  void operator=(const T& v) const {
    assert(cell_ && "State handle is empty (default-constructed or moved-from)");
    cell_->value = v;
  }
  void operator=(T&& v) const {
    assert(cell_ && "State handle is empty (default-constructed or moved-from)");
    cell_->value = std::move(v);
  }
  const T& operator*() const { return get(); }
  const T* operator->() const { return &get(); }
  std::uint32_t id() const { return cell_ ? cell_->id : 0; }

 private:
  std::shared_ptr<Cell<T>> cell_;
};

class Store {
 public:
  template <class T, class... A>
  State<T> make(A&&... a) {
    std::uint32_t id = next_.fetch_add(1);
    return State<T>(std::make_shared<Cell<T>>(T(std::forward<A>(a)...), id));
  }

 private:
  std::atomic<std::uint32_t> next_{1};
};

}  // namespace toolkit
}  // namespace glim
