#include <glim/image/Image.h>

namespace glim::image {

Diff compare(const Image& a, const Image& b, int maxChannelDelta) {
    Diff diff;
    if (a.empty() || b.empty() || a.width != b.width || a.height != b.height || a.rgba.size() != b.rgba.size()) {
        return diff;
    }
    diff.sameSize = true;
    const int maxD = maxChannelDelta < 0 ? 0 : maxChannelDelta;
    for (std::size_t i = 0; i < a.rgba.size(); ++i) {
        const int d = static_cast<int>(a.rgba[i]) - static_cast<int>(b.rgba[i]);
        if (d > maxD || d < -maxD) {
            ++diff.overDelta;
        }
    }
    return diff;
}

}  // namespace glim::image
