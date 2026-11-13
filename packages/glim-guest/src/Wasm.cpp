#include <glim/guest/Wasm.h>

#include <cstdint>

extern "C" {
__attribute__((import_module("glim"), import_name("host_send"))) void host_send();
__attribute__((import_module("glim"), import_name("host_now"))) std::uint64_t host_now();
__attribute__((import_module("glim"), import_name("host_log"))) void host_log(
    std::uint32_t level, const char* text, std::uint32_t length
);
}

namespace glim::guest {

namespace {

constexpr std::size_t kPageSize = 65536;

class HostImports final : public transport::Imports {
 public:
  void send() override { host_send(); }
  std::uint64_t now() override { return host_now(); }
  void log(std::uint32_t level, std::string_view text) override {
    host_log(level, text.data(), static_cast<std::uint32_t>(text.size()));
  }
};

}  // namespace

transport::Imports& hostImports() {
  static HostImports imports;
  return imports;
}

std::span<std::byte> linearMemory() {
  return {
      reinterpret_cast<std::byte*>(std::uintptr_t{0}), __builtin_wasm_memory_size(0) * kPageSize
  };
}

}  // namespace glim::guest
