#pragma once

#include <cstdint>
#include <utility>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Toolkit-side reservation for a host native; Slot is the Graphics/Host mechanism.
// Laid-out place box doubles as the IME anchor; candidate window stays app-owned.
struct Seat : WithSemantic {
  static constexpr const char* kKind = "Seat";
  const char* kind() const { return kKind; }
  std::uint32_t id = 0;
  float width = 0;
  float height = 0;
  Box place(Constraints c, const Engine&) const { return detail::resolveSize(width, height, c); }
  // Focusable anchor; key handling lives in the native view.
  bool handle(const Event& ev) const { return ev.is<PointerDown>() || ev.is<Activate>(); }
};

template <class... As>
auto seat(Seat s, As&&... as) {
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
