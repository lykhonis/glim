// Comments and strings mentioning Block pinned(int a, int b) are ignored.
namespace glim {
struct Button {
  int gap = 0;
  Block build(int a, int b) const;
};

template <class... As>
Block button(Button b, As&&... as) {
  return finish(std::move(b), std::forward<As>(as)...);
}

template <class F>
auto list(int count, F gen) {
  return count;
}

inline Block spacer() { return Block{}; }

struct ShadowTag {
  float sigma = 16.f;
};
inline ShadowTag shadow(ShadowTag s = {}) { return s; }
inline PadTag pad(int h, int v) { return {h, v}; }

inline Block lifted() {
  const ShadowTag lift = shadow({.sigma = 24.f});
  std::optional<ShadowTag> held = lift;
  return column(lift, is_tag<ShadowTag>{});
}

inline Block tall() {
  return column(shadow({
    .sigma = 24.f,
  }));
}
}  // namespace glim
