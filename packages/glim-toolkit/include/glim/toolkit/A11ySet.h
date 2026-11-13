#pragma once

#include <cstdint>
#include <initializer_list>

namespace glim {
namespace toolkit {

template <class E>
struct A11ySet {
  std::uint32_t bits = 0;
  constexpr A11ySet() = default;
  constexpr A11ySet(std::initializer_list<E> es) {
    for (E e : es) bits |= bit(e);
  }
  static constexpr std::uint32_t bit(E e) { return 1u << static_cast<std::uint32_t>(e); }
  constexpr bool has(E e) const { return (bits & bit(e)) != 0; }
  constexpr bool empty() const { return bits == 0; }
  constexpr A11ySet with(E e, bool on = true) const {
    A11ySet s = *this;
    s.bits = on ? (bits | bit(e)) : (bits & ~bit(e));
    return s;
  }
  constexpr A11ySet operator|(A11ySet o) const {
    A11ySet s;
    s.bits = bits | o.bits;
    return s;
  }
  constexpr bool operator==(A11ySet o) const { return bits == o.bits; }
};

// OS accessibility preferences; the host keeps them on the Store.
enum class A11yPref : std::uint8_t { ReduceMotion, HighContrast };
using A11yPrefs = A11ySet<A11yPref>;

}  // namespace toolkit
}  // namespace glim
