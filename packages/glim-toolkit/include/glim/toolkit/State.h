#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

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
  // Immutable-style update: one revision bump.
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

 private:
  friend class Store;
  // Hidden so callers hold a Watch by value instead of retaining raw ids.
  std::uint32_t id() const { return cell_ ? cell_->id : 0; }

  void bump() const {
    if (cell_ && cell_->revision) cell_->revision->fetch_add(1);
  }
  std::shared_ptr<Cell<T>> cell_;
};

class Store {
 public:
  Store(const Store&) = delete;
  Store& operator=(const Store&) = delete;
  Store(Store&&) = default;
  Store& operator=(Store&&) = default;
  Store() : revision_(std::make_shared<std::atomic<std::uint64_t>>(0)) {}
  template <class T, class... A>
  State<T> make(A&&... a) {
    std::uint32_t id = watchState()->next.fetch_add(1);
    return State<T>(std::make_shared<Cell<T>>(T(std::forward<A>(a)...), id, revision_));
  }
  // Scoped get-or-make: allocation never bumps; only set does.
  template <class T>
  State<T> state(const std::string& key, T init) {
    auto it = slots_.find(key);
    if (it != slots_.end()) {
      assert(it->second.type && *it->second.type == typeid(T));
      return State<T>(std::static_pointer_cast<Cell<T>>(it->second.ptr));
    }
    std::uint32_t id = watchState()->next.fetch_add(1);
    auto cell = std::make_shared<Cell<T>>(std::move(init), id, revision_);
    slots_.emplace(key, Slot{cell, &typeid(T)});
    return State<T>(cell);
  }
  std::uint64_t revision() const { return revision_->load(); }

  // Frame-pump seam: a value-step recomputes one cell at `now`.
  // Ownership: Watch tokens are RAII; destroying the token unregisters the step.
  using ValueStep = std::function<bool(float now)>;
  struct WatchState {
    std::atomic<std::uint32_t> next{1};
    std::unordered_map<std::uint32_t, ValueStep> watched;
  };
  class Watch {
   public:
    Watch() = default;
    Watch(const Watch&) = delete;
    Watch& operator=(const Watch&) = delete;
    Watch(Watch&& o) noexcept : state_(std::move(o.state_)), id_(o.id_) { o.id_ = 0; }
    Watch& operator=(Watch&& o) noexcept {
      if (this != &o) {
        const bool same = id_ != 0 && id_ == o.id_ && !state_.owner_before(o.state_) &&
                          !o.state_.owner_before(state_);
        if (!same) reset();
        state_ = std::move(o.state_);
        id_ = o.id_;
        o.id_ = 0;
      }
      return *this;
    }
    ~Watch() { reset(); }
    void reset() {
      if (auto s = state_.lock()) s->watched.erase(id_);
      state_.reset();
      id_ = 0;
    }
    bool live() const {
      if (auto s = state_.lock()) return s->watched.find(id_) != s->watched.end();
      return false;
    }

   private:
    friend class Store;
    Watch(std::weak_ptr<WatchState> s, std::uint32_t id) : state_(std::move(s)), id_(id) {}
    std::weak_ptr<WatchState> state_;
    std::uint32_t id_ = 0;
  };
  // Bind a step to a cell; re-arming replaces it.
  template <class T>
  Watch watchCell(const State<T>& cell, ValueStep step) {
    const std::uint32_t id = cell.id();
    assert(id && "watchCell needs a live State cell");
    watchState()->watched[id] = std::move(step);
    return Watch(watchState(), id);
  }
  Watch watchFrame(ValueStep step) {
    auto s = watchState();
    const std::uint32_t id = s->next.fetch_add(1);
    s->watched[id] = std::move(step);
    return Watch(std::move(s), id);
  }
  bool valuesLive() const { return watchState_ && !watchState_->watched.empty(); }
  void stepValues(float now) {
    if (!watchState_) return;
    auto& watched = watchState_->watched;
    std::vector<std::pair<std::uint32_t, ValueStep>> active;
    active.reserve(watched.size());
    for (auto& [id, fn] : watched) active.emplace_back(id, fn);
    std::vector<std::uint32_t> done;
    for (auto& [id, fn] : active) {
      if (watched.find(id) == watched.end()) continue;
      if (!fn(now)) done.push_back(id);
    }
    for (auto id : done) watched.erase(id);
  }

 private:
  const std::shared_ptr<WatchState>& watchState() {
    if (!watchState_) watchState_ = std::make_shared<WatchState>();
    return watchState_;
  }
  struct Slot {
    std::shared_ptr<void> ptr;
    const std::type_info* type = nullptr;
  };
  std::shared_ptr<std::atomic<std::uint64_t>> revision_;
  std::unordered_map<std::string, Slot> slots_;
  std::shared_ptr<WatchState> watchState_;
};

using StoreRef = std::optional<std::reference_wrapper<Store>>;

}  // namespace toolkit
}  // namespace glim
