namespace glim {
inline Block pinned(float width, float height, int pins) { return {}; }

template <class... As>
Block gone(Button b, As&&... as) {
  return finish(std::move(b));
}

template <class... As>
Block lost(Button b, As&&... as) {
  configure(b, std::forward<As>(as)...);
  return Block{};
}
inline ShadowTag drop(float sigma, float dy) { return {sigma, 0.f, dy}; }

inline Block plate() {
  GlassTag g;
  g.radius = 4;
  return column(g, RadiusTag{2});
}

inline Block tall() {
  return column(shadow(
    {.sigma = kLiftedCardShadowSigma,
     .offsetY = kLiftedCardShadowOffsetY,
     .spread = kLiftedCardShadowSpread,
     .color = kLiftedCardShadowColor}
  ));
}
}  // namespace glim
