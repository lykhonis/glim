#pragma once

#include <glim/guest_host/Session.h>
#include <glim/transport/GuestAbi.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace glim::runtime_wamr {

// Process-wide WAMR state and the "glim" host imports. Create one before
// any Instance and keep it alive longer than all of them.
class Engine {
 public:
  Engine();
  ~Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
};

// One guest module in the WAMR interpreter (K81). Throws std::runtime_error
// when the module fails to load or a call traps.
class Instance final : public guest_host::Runtime {
 public:
  struct Limits {
    std::uint32_t stack = 64 * 1024;
  };

  Instance(Engine& engine, std::span<const std::byte> module, Limits limits);
  ~Instance() override;
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  // Routes the guest's imports; a guest calling one before this traps.
  void attach(transport::Imports& host);

  std::span<std::byte> memory() override;
  std::uint32_t init(std::uint32_t protocol, std::uint32_t capacity) override;
  void recv() override;
  void tick(std::uint64_t now) override;

 private:
  struct Vm;
  std::vector<std::uint8_t> module_;
  std::unique_ptr<Vm> vm_;
};

}  // namespace glim::runtime_wamr
