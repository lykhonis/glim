#include <cstdint>

// Non-macOS stub: track-only, no native view. positionSlot on these
// shells records the id; there is nothing to show.
namespace glim {
namespace toolkit {
namespace paint {

void* glimMakeSeatView(std::uint32_t) { return nullptr; }

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
