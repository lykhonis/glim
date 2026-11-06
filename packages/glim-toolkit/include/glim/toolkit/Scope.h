#pragma once

// Scoped allocation at the use site: state lives next to usage. Positional shares the React rule.
#include <cassert>
#include <memory>
#include <string>
#include <typeinfo>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"

namespace glim {
namespace toolkit {

struct Scope {
  StoreRef store;
  std::string prefix;
  std::shared_ptr<int> cursor;

  Scope() = default;
  Scope(StoreRef s, std::string prefix)
      : store(std::move(s)), prefix(std::move(prefix)), cursor(std::make_shared<int>(0)) {}

  Scope child(const std::string& name) const {
    std::string p = prefix.empty() ? name : prefix + "/" + name;
    return Scope(store, std::move(p));
  }

  template <class T>
  State<T> state(const std::string& key, T init) const {
    assert(store && "Scope has no Store (build it from an Engine carrying one)");
    std::string full = prefix.empty() ? key : prefix + "/" + key;
    return store->get().state(full, std::move(init));
  }

  template <class Tag, class T>
  State<T> state(T init) const {
    assert(store && "Scope has no Store (build it from an Engine carrying one)");
    std::string tag = std::string("#") + typeid(Tag).name();
    std::string full = prefix.empty() ? tag : prefix + "/" + tag;
    return store->get().state(full, std::move(init));
  }

  template <class T>
  State<T> next(T init) const {
    assert(store && cursor && "Scope has no Store (build it from an Engine carrying one)");
    std::string full = prefix.empty() ? "#" + std::to_string((*cursor)++)
                                      : prefix + "/#" + std::to_string((*cursor)++);
    return store->get().state(full, std::move(init));
  }
};

inline Scope rootScope(const Engine& e) { return Scope(e.store, ""); }

}  // namespace toolkit
}  // namespace glim
