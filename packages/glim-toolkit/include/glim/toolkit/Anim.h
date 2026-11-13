#pragma once

#include <cassert>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "glim/toolkit/Motion.h"
#include "glim/toolkit/Scope.h"
#include "glim/toolkit/State.h"

namespace glim {
namespace toolkit {

template <class T>
struct Anim {
  Anim() = default;
  // Binds a cell in the app scope and registers nothing until the first
  // set.
  Anim(const Scope& scope, const std::string& key, T init, AnimSpec spec = AnimSpec{})
      : store_(scope.store) {
    assert(store_ && "Anim needs a Scope built from an Engine carrying a Store");
    assert(spec.ease && "AnimSpec needs an ease function");
    core_ = std::make_shared<Core>(Core{scope.state(key, init), init, init, spec, store_});
  }
  Anim(std::string key, T init, AnimSpec spec = {})
      : key_(std::move(key)), init_(std::move(init)), spec_(spec) {}

  // Displayed value: what paint reads.
  const T& get() const {
    ensure();
    assert((core_ || !key_.empty()) && "Anim is unbound (default-constructed, never assigned)");
    return core_ ? core_->cell.get() : init_;
  }
  const T& operator*() const { return get(); }
  // Goal value: what logic reads.
  const T& target() const {
    ensure();
    assert((core_ || !key_.empty()) && "Anim is unbound (default-constructed, never assigned)");
    return core_ ? core_->target : init_;
  }
  bool live() const { return core_ && core_->live; }
  long refs() const { return core_ ? static_cast<long>(core_.use_count()) : 1; }
  const AnimSpec& spec() const { return core_ ? core_->spec : spec_; }
  void setSpec(AnimSpec s) const {
    assert(s.ease && "AnimSpec needs an ease function");
    if (core_)
      core_->spec = s;
    else
      spec_ = s;
  }

  // Animate toward t; anchors on the next pump step.
  void set(const T& t) const {
    if (!ensure()) {
      assert(!key_.empty() && "Anim is unbound (default-constructed, never assigned)");
      init_ = t;
      return;
    }
    set(t, core_->spec);
  }
  void set(const T& t, AnimSpec s) const {
    if (!ensure()) {
      assert(!key_.empty() && "Anim is unbound (default-constructed, never assigned)");
      init_ = t;
      spec_ = s;
      return;
    }
    assert(store_ && "Anim::set needs a Store (construct from a Scope or wait for a walk)");
    assert(s.ease && "AnimSpec needs an ease function");
    assert(!s.spring || s.damping > 0.0f);
    core_->spec = s;
    // Already there: keep the spec, skip the pump.
    if (!core_->live && t == core_->cell.get()) return;
    core_->from = core_->cell.get();
    core_->target = t;
    // Spring keeps velocity (physical handoff); ease restarts. No poke
    // here: followers would resync to a stale value; they wake per-step
    // instead.
    core_->live = true;
    core_->started = false;
    core_->watch =
        store_->get().watchCell(core_->cell, [c = core_](float now) { return c->step(now); });
  }
  void operator=(const T& t) const { set(t); }
  // Jump, cancelling anything live.
  void snap(const T& v) const {
    if (!ensure()) {
      assert(!key_.empty() && "Anim is unbound (default-constructed, never assigned)");
      init_ = v;
      return;
    }
    core_->live = false;
    core_->from = v;
    core_->target = v;
    core_->vel = 0.0f;
    if (v != core_->cell.get()) core_->cell = v;
    core_->poke();
    core_->watch.reset();
  }
  // One pump step. Public for unit tests; the Store drives it.
  bool step(float now) const {
    if (!core_) return false;
    return core_->step(now);
  }

  // Chain: track another Anim declaratively; the sink coasts behind on
  // its own spec. Links are acyclic by contract — a cycle never
  // settles.
  void follow(const Anim& src) const {
    ensure();
    src.ensure();
    assert(
        core_ && src.core_ &&
        "Anim::follow needs bound Anims (construct from a Scope or wait for a walk)"
    );
    core_->src = src.core_;
    std::weak_ptr<Core> w = core_;
    std::weak_ptr<Core> wsrc = src.core_;
    StoreRef st = store_;
    src.core_->followers.push_back([w, wsrc, st] {
      auto c = w.lock();
      auto s = wsrc.lock();
      if (!c || !s || c->src != s) return;
      c->resync();
      if (c->live) {
        c->watch = st->get().watchCell(c->cell, [c](float now) { return c->step(now); });
      }
    });
    // Catch up to the source.
    set(src.get());
  }

 private:
  struct Core {
    State<T> cell;
    T from;
    T target;
    AnimSpec spec;
    StoreRef store;
    // Spring state: velocity persists across re-targets; prev bounds
    // the step.
    float vel = 0.0f;
    float start = 0.0f;
    float prev = 0.0f;
    bool live = false;
    bool started = false;
    // The watch token owns the Store registration: re-arming
    // move-assigns it.
    std::shared_ptr<Core> src = nullptr;
    std::vector<std::function<void()>> followers = {};
    Store::Watch watch = {};
    void poke() {
      for (auto& f : followers) f();
    }
    // Re-sync to the linked source: re-anchor, register only when
    // moving.
    void resync() {
      if (!src) return;
      const T s = src->cell.get();
      if (!live && s == cell.get()) return;
      from = cell.get();
      target = s;
      live = true;
      started = false;
    }
    bool step(float now) {
      if (!live) return false;
      if (!started) {
        started = true;
        start = now;
        prev = now;
      }
      // Linked sink: chase the source's displayed value; settles only
      // after the source stops, so nothing strands mid-chain.
      if (src) {
        const T s = src->cell.get();
        if (s != target) {
          target = s;
          from = cell.get();
          start = now;
          prev = now;
        }
      }
      bool keep;
      const bool instant = store && store->get().prefs().has(A11yPref::ReduceMotion);
      const float dt = now - start - spec.delay;
      if (instant) {
        if (target != cell.get()) cell = target;
        vel = 0.0f;
        live = false;
        keep = false;
      } else if (dt < 0.0f) {
        prev = now;
        keep = true;
      } else if (spec.spring) {
        keep = stepSpring(now);
      } else {
        T v;
        if (spec.dur <= 0.0f || dt >= spec.dur) {
          v = target;
          live = false;
        } else {
          v = from + (target - from) * spec.ease(dt / spec.dur);
        }
        if (v != cell.get()) cell = v;
        keep = live;
      }
      // Wake downstream every live step, so the final snap propagates
      // too.
      poke();
      return keep || (src && src->live);
    }
    // Semi-implicit Euler at up to two substeps; the 1/30 clamp keeps
    // tab-switch gaps stable. Settles by threshold, then snaps exact.
    bool stepSpring(float now) {
      float h = now - prev;
      prev = now;
      if (h <= 0.0f) return true;
      if (h > 1.0f / 30.0f) h = 1.0f / 30.0f;
      float x = cell.get();
      float v = vel;
      for (int i = 0; i < 2; ++i) {
        const float hh = h * 0.5f;
        const float f = -spec.stiffness * (x - target) - spec.damping * v;
        v += f * hh;
        x += v * hh;
      }
      if (std::fabs(x - target) < 0.01f && std::fabs(v) < 1.0f) {
        x = target;
        v = 0.0f;
        live = false;
      }
      vel = v;
      if (x != cell.get()) cell = x;
      return live;
    }
  };
  // Handle: const-assignable like State.
  bool ensure() const {
    if (core_) return true;
    if (key_.empty() || !detail::AutoCtx::active() || !detail::AutoCtx::store()) return false;
    std::string k = "#auto/";
    for (int p : detail::AutoCtx::path()) {
      k += std::to_string(p);
      k += '/';
    }
    k += key_;
    State<T> cell = detail::AutoCtx::store()->state(k, init_);
    store_ = std::ref(*detail::AutoCtx::store());
    core_ = std::make_shared<Core>(Core{cell, cell.get(), cell.get(), spec_, store_});
    return true;
  }
  mutable std::shared_ptr<Core> core_;
  mutable StoreRef store_;
  std::string key_;
  mutable T init_ = {};
  mutable AnimSpec spec_ = {};
};

}  // namespace toolkit
}  // namespace glim
