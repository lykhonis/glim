#include <glim/toolkit/host/SeatView.h>

// Track-only shells (Wayland, Android, software smoke): no native view.
// Android child windows and Wayland subsurfaces stay embedder-owned;
// returning null keeps geometry tracking without creating a view.
namespace glim {
namespace toolkit {
namespace host {

void* makeDefaultSeatView(SeatRequest) { return nullptr; }

}  // namespace host
}  // namespace toolkit
}  // namespace glim
