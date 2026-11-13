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

#include "glim/toolkit/A11ySet.h"

namespace glim {
namespace toolkit {

class Store;

using StoreRef = std::optional<std::reference_wrapper<Store>>;

namespace detail {
// Walk-provided allocation context for lazy handles; unset outside walks.
struct AutoCtx {
  static Store*& store() {
    thread_local Store* s = nullptr;
    return s;
  }
  static std::vector<int>& path() {
    thread_local std::vector<int> p;
    return p;
  }
  static bool& active() {
    thread_local bool a = false;
    return a;
  }
  static std::vector<int>& found() {
    thread_local std::vector<int> f;
    return f;
  }
};
struct AutoGuard {
  explicit AutoGuard(Store* s)
      : store_(AutoCtx::store()), active_(AutoCtx::active()), depth_(AutoCtx::path().size()) {
    AutoCtx::store() = s;
    AutoCtx::active() = true;
  }
  ~AutoGuard() {
    AutoCtx::store() = store_;
    AutoCtx::active() = active_;
    AutoCtx::path().resize(depth_);
  }
  Store* store_;
  bool active_;
  std::size_t depth_;
};
struct PathPush {
  explicit PathPush(int k) { AutoCtx::path().push_back(k); }
  ~PathPush() { AutoCtx::path().pop_back(); }
};
inline void noteHit() { AutoCtx::found() = AutoCtx::path(); }
struct AutoHandle {
  AutoHandle(Store* s, const std::vector<int>& path)
      : store_(AutoCtx::store()), active_(AutoCtx::active()), path_(AutoCtx::path()) {
    AutoCtx::store() = s;
    AutoCtx::active() = true;
    AutoCtx::path() = path;
  }
  ~AutoHandle() {
    AutoCtx::store() = store_;
    AutoCtx::active() = active_;
    AutoCtx::path() = std::move(path_);
  }
  Store* store_;
  bool active_;
  std::vector<int> path_;
};
}  // namespace detail

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
  State(std::string key, T init) : key_(std::move(key)), init_(std::move(init)) {}
  explicit operator bool() const { return static_cast<bool>(cell_); }
  bool operator==(const State& o) const { return cell_ == o.cell_; }
  bool operator!=(const State& o) const { return cell_ != o.cell_; }
  const T& get() const {
    ensure();
    assert((cell_ || !key_.empty()) && "State handle is empty (default-constructed or moved-from)");
    return cell_ ? cell_->value : init_;
  }
  void set(const T& v) const {
    if (!ensure()) {
      assert(!key_.empty() && "State handle is empty (default-constructed or moved-from)");
      init_ = v;
      return;
    }
    cell_->value = v;
    bump();
  }
  void set(T&& v) const {
    if (!ensure()) {
      assert(!key_.empty() && "State handle is empty (default-constructed or moved-from)");
      init_ = std::move(v);
      return;
    }
    cell_->value = std::move(v);
    bump();
  }
  void operator=(const T& v) const { set(v); }
  void operator=(T&& v) const { set(std::move(v)); }
  // Immutable-style update: one revision bump.
  template <class F>
  void update(F&& fn) const {
    if (!ensure()) {
      assert(!key_.empty() && "State handle is empty (default-constructed or moved-from)");
      T next = init_;
      fn(next);
      init_ = std::move(next);
      return;
    }
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

  bool ensure() const;

  void bump() const {
    if (cell_ && cell_->revision) cell_->revision->fetch_add(1);
  }
  mutable std::shared_ptr<Cell<T>> cell_;
  mutable std::string key_;
  mutable T init_ = {};
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
    return slot<T>(key, std::move(init));
  }
  std::uint64_t revision() const { return revision_->load(); }
  A11yPrefs prefs() const { return prefs_; }
  void setPrefs(A11yPrefs p) {
    if (p == prefs_) return;
    prefs_ = p;
    revision_->fetch_add(1);
  }

  // Frame-pump seam: a value-step recomputes one cell at `now`.
  // Ownership: Watch tokens are RAII; destroying the token unregisters the step.
  using ValueStep = std::function<bool(float now)>;
  struct WatchState {
    std::atomic<std::uint32_t> next{1};
    std::unordered_map<std::uint32_t, ValueStep> watched;
    // Current token per watched id: a re-armed cell keeps its id, so
    // only the newest Watch may unregister it.
    std::unordered_map<std::uint32_t, std::uint32_t> tokens;
  };
  class Watch {
   public:
    Watch() = default;
    Watch(const Watch&) = delete;
    Watch& operator=(const Watch&) = delete;
    Watch(Watch&& o) noexcept : state_(std::move(o.state_)), id_(o.id_), token_(o.token_) {
      o.id_ = 0;
      o.token_ = 0;
    }
    Watch& operator=(Watch&& o) noexcept {
      if (this != &o) {
        reset();
        state_ = std::move(o.state_);
        id_ = o.id_;
        token_ = o.token_;
        o.id_ = 0;
        o.token_ = 0;
      }
      return *this;
    }
    ~Watch() { reset(); }
    void reset() {
      // Erasing a step can destroy its captures, which may own other
      // Watches that reset into the same maps: take it out first.
      ValueStep dropped;
      if (auto s = state_.lock()) {
        if (current(*s)) {
          auto it = s->watched.find(id_);
          if (it != s->watched.end()) {
            dropped = std::move(it->second);
            s->watched.erase(it);
          }
          s->tokens.erase(id_);
        }
      }
      state_.reset();
      id_ = 0;
      token_ = 0;
    }
    bool live() const {
      if (auto s = state_.lock()) return current(*s) && s->watched.count(id_) != 0;
      return false;
    }

   private:
    friend class Store;
    Watch(std::weak_ptr<WatchState> s, std::uint32_t id, std::uint32_t token)
        : state_(std::move(s)), id_(id), token_(token) {}
    bool current(const WatchState& s) const {
      auto it = s.tokens.find(id_);
      return it != s.tokens.end() && it->second == token_;
    }
    std::weak_ptr<WatchState> state_;
    std::uint32_t id_ = 0;
    std::uint32_t token_ = 0;
  };
  // Bind a step to a cell; re-arming replaces it and retires the old
  // Watch token, so the old owner going away leaves the new step alone.
  template <class T>
  Watch watchCell(const State<T>& cell, ValueStep step) {
    const std::uint32_t id = cell.id();
    assert(id && "watchCell needs a live State cell");
    return arm(id, std::move(step));
  }
  Watch watchFrame(ValueStep step) { return arm(watchState()->next.fetch_add(1), std::move(step)); }
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
    for (auto id : done) {
      watched.erase(id);
      watchState_->tokens.erase(id);
    }
  }

 private:
  Watch arm(std::uint32_t id, ValueStep step) {
    auto s = watchState();
    const std::uint32_t token = s->next.fetch_add(1);
    // The replaced step dies last, after the new token is current, so
    // its captured Watches see themselves stale and erase nothing.
    ValueStep replaced;
    ValueStep& slot = s->watched[id];
    replaced.swap(slot);
    slot = std::move(step);
    s->tokens[id] = token;
    return Watch(std::move(s), id, token);
  }
  template <class T>
  State<T> slot(const std::string& key, T init) {
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
  const std::shared_ptr<WatchState>& watchState() {
    if (!watchState_) watchState_ = std::make_shared<WatchState>();
    return watchState_;
  }
  struct Slot {
    std::shared_ptr<void> ptr;
    const std::type_info* type = nullptr;
  };
  std::shared_ptr<std::atomic<std::uint64_t>> revision_;
  A11yPrefs prefs_;
  std::unordered_map<std::string, Slot> slots_;
  std::shared_ptr<WatchState> watchState_;
};

template <class T>
bool State<T>::ensure() const {
  if (cell_) return true;
  if (key_.empty() || !detail::AutoCtx::active() || !detail::AutoCtx::store()) return false;
  std::string k = "#auto/";
  for (int p : detail::AutoCtx::path()) {
    k += std::to_string(p);
    k += '/';
  }
  k += key_;
  State<T> bound = detail::AutoCtx::store()->state(k, init_);
  cell_ = bound.cell_;
  return true;
}

}  // namespace toolkit
}  // namespace glim
