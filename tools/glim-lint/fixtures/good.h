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
}  // namespace glim
