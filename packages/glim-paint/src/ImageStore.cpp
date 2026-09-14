#include <glim/paint/Scene.h>

#include <cstdint>
#include <cstring>

namespace glim::paint {

ImageStore::ImageStore() : data_(std::make_shared<Data>()) {}

std::uint32_t ImageStore::add(int width, int height, const std::uint8_t* rgba) {
    if (!data_ || !rgba || width <= 0 || height <= 0) {
        return 0;
    }
    if (width > kMaxImageSide || height > kMaxImageSide) {
        return 0;
    }
    const std::uint64_t bytes =
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4ull;
    if (bytes > kMaxImageBytes) {
        return 0;
    }
    StoredImage img;
    img.width = width;
    img.height = height;
    img.rgba.resize(static_cast<std::size_t>(bytes));
    std::memcpy(img.rgba.data(), rgba, static_cast<std::size_t>(bytes));
    data_->slots.push_back(std::move(img));
    return static_cast<std::uint32_t>(data_->slots.size() - 1);
}

void ImageStore::release(std::uint32_t id) {
    if (!data_ || id == 0 || id >= data_->slots.size()) {
        return;
    }
    data_->slots[id] = StoredImage{};
}

const StoredImage* ImageStore::get(std::uint32_t id) const {
    if (!data_ || id == 0 || id >= data_->slots.size()) {
        return nullptr;
    }
    const StoredImage& img = data_->slots[id];
    if (img.width <= 0 || img.height <= 0 || img.rgba.empty()) {
        return nullptr;
    }
    return &img;
}

}  // namespace glim::paint
