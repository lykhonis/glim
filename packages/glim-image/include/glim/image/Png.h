#pragma once

#include <glim/image/Image.h>

namespace glim::image {

class Png {
public:
    Png() = delete;

    static bool read(const char* path, Image* out);
    static bool write(const char* path, const Image& image);
};

}  // namespace glim::image
