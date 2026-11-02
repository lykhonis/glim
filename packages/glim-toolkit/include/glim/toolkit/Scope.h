#pragma once

// Scoped state allocation at the use site. The app owns its Store;
// build functions borrow it through the Engine and allocate where
// they render, so state lives next to usage instead of a far-away
// pre-allocation struct. No globals: every accessor takes the Scope
// explicitly, and scopes are per-build temporaries.
//
// Three spellings, one map:
//   scope.state("level", 0.4f)  explicit key for dynamic lists
//   scope.state<LevelTag>(0.4f) type key, compiler-checked, preferred
//   scope.next(0.4f)            positional, order-stable like hooks
// Positional shares the React rule: same call order every build, no
// conditionals around it. Child scopes namespace keys and reset the
// positional cursor, so nested components compose.
#include <cassert>
#include <memory>
#include <string>
#include <typeinfo>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"

namespace glim {
namespace toolkit {

struct Scope {
  Store* store = nullptr;
  std::string prefix;
  std::shared_ptr<int> cursor;

  Scope() = default;
  Scope(Store* s, std::string prefix)
      : store(s), prefix(std::move(prefix)), cursor(std::make_shared<int>(0)) {}

  Scope child(const std::string& name) const {
    std::string p = prefix.empty() ? name : prefix + "/" + name;
    return Scope(store, std::move(p));
  }

  template <class T>
  State<T> state(const std::string& key, T init) const {
    assert(store && "Scope has no Store (build it from an Engine carrying one)");
    std::string full = prefix.empty() ? key : prefix + "/" + key;
    return store->state(full, std::move(init));
  }

  template <class Tag, class T>
  State<T> state(T init) const {
    assert(store && "Scope has no Store (build it from an Engine carrying one)");
    std::string tag = std::string("#") + typeid(Tag).name();
    std::string full = prefix.empty() ? tag : prefix + "/" + tag;
    return store->state(full, std::move(init));
  }

  template <class T>
  State<T> next(T init) const {
    assert(store && cursor && "Scope has no Store (build it from an Engine carrying one)");
    std::string full = prefix.empty() ? "#" + std::to_string((*cursor)++)
                                      : prefix + "/#" + std::to_string((*cursor)++);
    return store->state(full, std::move(init));
  }
};

inline Scope rootScope(const Engine& e) { return Scope(e.store, ""); }

}  // namespace toolkit
}  // namespace glim
