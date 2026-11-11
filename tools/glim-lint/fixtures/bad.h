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
}  // namespace glim
