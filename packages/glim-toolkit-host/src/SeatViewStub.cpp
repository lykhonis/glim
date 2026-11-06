#include <glim/toolkit/host/SeatView.h>

// Track-only shells (Wayland, Android, software smoke): no native view;
// geometry tracking continues with a null view.
namespace glim {
namespace toolkit {
namespace host {

void* makeDefaultSeatView(SeatRequest) { return nullptr; }

}  // namespace host
}  // namespace toolkit
}  // namespace glim
