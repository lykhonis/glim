#pragma once

#include <glim/transport/Chunk.h>

#include <coroutine>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace glim::transport {

enum class Outcome { Ok, Error, Timeout, Reset };

struct Reply {
  Outcome outcome = Outcome::Reset;
  std::vector<std::byte> payload;
};

// Pending requests. Each ends exactly once: settled and resumed, or
// dropped with its owner and destroyed without resuming.
class Requests {
 public:
  struct Entry {
    const void* owner;
    Lane lane;
    std::uint64_t deadline;
    std::coroutine_handle<> handle;
    Reply* slot;
  };

  Requests() = default;
  Requests(const Requests&) = delete;
  Requests& operator=(const Requests&) = delete;
  ~Requests();

  struct Ended {
    std::uint32_t id;
    Lane lane;
  };

  std::uint32_t add(Entry entry);
  void remove(std::uint32_t id);
  bool settle(std::uint32_t id, Reply reply);
  std::vector<Ended> expire(std::uint64_t now);
  void failAll();
  std::vector<Ended> drop(const void* owner);
  bool empty() const { return pending_.empty(); }

 private:
  std::unordered_map<std::uint32_t, Entry> pending_;
  std::uint32_t next_ = 1;
};

}  // namespace glim::transport
