#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_map>

namespace glim {
namespace toolkit {

template <class T>
struct Cell {
  T value;
  std::uint32_t id;
  std::shared_ptr<std::atomic<std::uint64_t>> revision;
  Cell(T v, std::uint32_t i, std::shared_ptr<std::atomic<std::uint64_t>> r)
      : value(std::move(v)), id(i), revision(std::move(r)) {}
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
    bump();
  }
  void set(T&& v) const {
    assert(cell_ && "State handle is empty (default-constructed or moved-from)");
    cell_->value = std::move(v);
    bump();
  }
  void operator=(const T& v) const {
    assert(cell_ && "State handle is empty (default-constructed or moved-from)");
    cell_->value = v;
    bump();
  }
  void operator=(T&& v) const {
    assert(cell_ && "State handle is empty (default-constructed or moved-from)");
    cell_->value = std::move(v);
    bump();
  }
  // Immutable-style update: copies the value, runs the mutator on
  // the copy, writes it back. One revision bump; the write is
  // explicit at the call site. For struct/class state, prefer this
  // over copy-out/mutate/assign.
  template <class F>
  void update(F&& fn) const {
    assert(cell_ && "State handle is empty (default-constructed or moved-from)");
    T next = cell_->value;
    fn(next);
    cell_->value = std::move(next);
    bump();
  }
  const T& operator*() const { return get(); }
  const T* operator->() const { return &get(); }
  std::uint32_t id() const { return cell_ ? cell_->id : 0; }

 private:
  void bump() const {
    if (cell_ && cell_->revision) cell_->revision->fetch_add(1);
  }
  std::shared_ptr<Cell<T>> cell_;
};

class Store {
 public:
  Store() : revision_(std::make_shared<std::atomic<std::uint64_t>>(0)) {}
  template <class T, class... A>
  State<T> make(A&&... a) {
    std::uint32_t id = next_.fetch_add(1);
    return State<T>(std::make_shared<Cell<T>>(T(std::forward<A>(a)...), id, revision_));
  }
  // Scoped get-or-make: first call with a key allocates, later calls
  // with the same key reuse the cell. Type mismatch across calls
  // asserts. Allocation does not bump the revision; only set does.
  template <class T>
  State<T> state(const std::string& key, T init) {
    auto it = slots_.find(key);
    if (it != slots_.end()) {
      assert(it->second.type && *it->second.type == typeid(T));
      return State<T>(std::static_pointer_cast<Cell<T>>(it->second.ptr));
    }
    std::uint32_t id = next_.fetch_add(1);
    auto cell = std::make_shared<Cell<T>>(std::move(init), id, revision_);
    slots_.emplace(key, Slot{cell, &typeid(T)});
    return State<T>(cell);
  }
  std::uint64_t revision() const { return revision_->load(); }

 private:
  struct Slot {
    std::shared_ptr<void> ptr;
    const std::type_info* type = nullptr;
  };
  std::atomic<std::uint32_t> next_{1};
  std::shared_ptr<std::atomic<std::uint64_t>> revision_;
  std::unordered_map<std::string, Slot> slots_;
};

}  // namespace toolkit
}  // namespace glim
