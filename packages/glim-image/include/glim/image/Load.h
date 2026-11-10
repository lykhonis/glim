#pragma once

#include <glim/image/Image.h>

namespace glim::image {

// PNG or JPEG by content (reference designs are often JPEG). RGBA8 out.
bool load(const char* path, Image* out);

}  // namespace glim::image
