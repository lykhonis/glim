#include <glim/transport/Requests.h>

#include <utility>

namespace glim::transport {

namespace {

void resumeAll(const std::vector<Requests::Entry>& done, Outcome outcome) {
  for (const auto& entry : done) {
    *entry.slot = Reply{outcome, {}};
  }
  for (const auto& entry : done) {
    entry.handle.resume();
  }
}

}  // namespace

Requests::~Requests() {
  auto pending = std::move(pending_);
  for (auto& [id, entry] : pending) {
    entry.handle.destroy();
  }
}

std::uint32_t Requests::add(Entry entry) {
  while (next_ == 0 || pending_.contains(next_)) {
    ++next_;
  }
  const auto id = next_++;
  pending_.emplace(id, entry);
  return id;
}

void Requests::remove(std::uint32_t id) { pending_.erase(id); }

bool Requests::settle(std::uint32_t id, Reply reply) {
  const auto it = pending_.find(id);
  if (it == pending_.end()) {
    return false;
  }
  const auto entry = it->second;
  pending_.erase(it);
  *entry.slot = std::move(reply);
  entry.handle.resume();
  return true;
}

std::vector<Requests::Ended> Requests::expire(std::uint64_t now) {
  std::vector<Ended> ids;
  std::vector<Entry> done;
  std::erase_if(pending_, [&](const auto& item) {
    const auto& [id, entry] = item;
    if (entry.deadline == 0 || entry.deadline > now) {
      return false;
    }
    ids.push_back({id, entry.lane});
    done.push_back(entry);
    return true;
  });
  resumeAll(done, Outcome::Timeout);
  return ids;
}

void Requests::failAll() {
  std::vector<Entry> done;
  for (const auto& [id, entry] : pending_) {
    done.push_back(entry);
  }
  pending_.clear();
  resumeAll(done, Outcome::Reset);
}

std::vector<Requests::Ended> Requests::drop(const void* owner) {
  std::vector<Ended> ids;
  std::vector<std::coroutine_handle<>> handles;
  std::erase_if(pending_, [&](const auto& item) {
    if (item.second.owner != owner) {
      return false;
    }
    ids.push_back({item.first, item.second.lane});
    handles.push_back(item.second.handle);
    return true;
  });
  for (const auto handle : handles) {
    handle.destroy();
  }
  return ids;
}

}  // namespace glim::transport
