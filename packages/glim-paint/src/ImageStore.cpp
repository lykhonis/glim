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

std::uint32_t ImageStore::wrap(
  void* native, int width, int height, SampleFormat format, NativeKind kind
) {
  if (!data_ || !native || width <= 0 || height <= 0) {
    return 0;
  }
  if (width > kMaxImageSide || height > kMaxImageSide) {
    return 0;
  }
  StoredImage img;
  img.width = width;
  img.height = height;
  img.native = native;
  img.format = format;
  img.nativeKind = kind;
  img.foreign = true;
  data_->slots.push_back(std::move(img));
  return static_cast<std::uint32_t>(data_->slots.size() - 1);
}

void ImageStore::release(std::uint32_t id) {
  if (!data_ || id == 0 || id >= data_->slots.size()) {
    return;
  }
  data_->slots[id] = StoredImage{};
}

std::optional<std::reference_wrapper<const StoredImage>> ImageStore::get(std::uint32_t id) const {
  if (!data_ || id == 0 || id >= data_->slots.size()) {
    return std::nullopt;
  }
  const StoredImage& img = data_->slots[id];
  if (img.width <= 0 || img.height <= 0) {
    return std::nullopt;
  }
  if (img.rgba.empty() && !img.native) {
    return std::nullopt;
  }
  return std::cref(img);
}

}  // namespace glim::paint
