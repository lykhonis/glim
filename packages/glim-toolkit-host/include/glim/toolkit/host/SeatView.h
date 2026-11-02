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

// Per-platform default. Apple builds return a plain text field, other
// shells return null (track-only). Embedders pass their own factory to
// host::run instead. Ownership: Glim never retains the pointer, so it
// must stay valid from attach until the Seat disappears — the default
// factory retains process-wide; custom factories must guarantee the
// same (e.g. owned by the embedder's own hierarchy).
void* makeDefaultSeatView(SeatRequest request);

}  // namespace host
}  // namespace toolkit
}  // namespace glim
