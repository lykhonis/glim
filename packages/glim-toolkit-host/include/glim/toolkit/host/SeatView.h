#pragma once

#include <glim/math.h>

#include <cstdint>

namespace glim {
namespace toolkit {
namespace host {

// What the host needs to create a native view for a Seat.
struct SeatRequest {
  std::uint32_t id = 0;
  Rect bounds{};
};

using SeatViewFactory = void* (*)(SeatRequest);

// Per-platform default. Ownership: Glim never retains the pointer — it must
// stay valid from attach until the Seat disappears (default retains process-wide).
void* makeDefaultSeatView(SeatRequest request);

}  // namespace host
}  // namespace toolkit
}  // namespace glim
