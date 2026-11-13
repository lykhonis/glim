#include <glim/transport/Endpoint.h>

#include <algorithm>
#include <utility>

namespace glim::transport {

class Endpoint::Sink final : public InboxSink {
 public:
  explicit Sink(Endpoint& endpoint) : endpoint_(endpoint) {}

  void deliver(const Message& message) override {
    if (message.kind == Kind::Reply || message.kind == Kind::Error) {
      endpoint_.requests_.settle(
          message.request,
          {message.kind == Kind::Reply ? Outcome::Ok : Outcome::Error,
           {message.payload.begin(), message.payload.end()}}
      );
      return;
    }
    endpoint_.handler_(message);
  }
  void release(Lane lane, std::uint32_t bytes) override {
    endpoint_.outbox_.returnCredit(lane, bytes);
  }
  bool granted(Lane lane, std::uint32_t bytes) override {
    return endpoint_.outbox_.grant(lane, bytes);
  }

 private:
  Endpoint& endpoint_;
};

Owner::Owner(Endpoint& endpoint) : endpoint_(&endpoint) { endpoint.owners_.push_back(this); }

Owner::~Owner() {
  if (endpoint_ != nullptr) {
    endpoint_->release(*this);
  }
}

bool Call::await_suspend(std::coroutine_handle<> handle) { return endpoint_.start(*this, handle); }

Endpoint::Endpoint(Transport& transport, Config config, Handler handler)
    : transport_(transport),
      config_(config),
      handler_(std::move(handler)),
      inbox_(config),
      outbox_(config) {}

Endpoint::~Endpoint() {
  for (auto* owner : owners_) {
    owner->endpoint_ = nullptr;
  }
}

bool Endpoint::post(
    Lane lane, std::uint16_t channel, std::vector<std::byte> payload, bool supersede
) {
  return !broken_ && outbox_.push(
                         {.lane = lane,
                          .kind = Kind::Event,
                          .channel = channel,
                          .supersede = supersede,
                          .payload = std::move(payload)}
                     );
}

Call Endpoint::request(
    Owner& owner,
    Lane lane,
    std::uint16_t channel,
    std::vector<std::byte> payload,
    std::uint64_t timeout
) {
  return Call(
      *this,
      owner,
      {.lane = lane, .kind = Kind::Request, .channel = channel, .payload = std::move(payload)},
      timeout
  );
}

bool Endpoint::start(Call& call, std::coroutine_handle<> handle) {
  if (broken_ || call.owner_.endpoint_ != this) {
    call.result_ = {Outcome::Reset, {}};
    return false;
  }
  const auto deadline = call.timeout_ == 0 ? 0 : now_ + call.timeout_;
  const auto id =
      requests_.add({&call.owner_, call.message_.lane, deadline, handle, &call.result_});
  call.message_.request = id;
  if (!outbox_.push(std::move(call.message_))) {
    requests_.remove(id);
    call.result_ = {Outcome::Error, {}};
    return false;
  }
  return true;
}

bool Endpoint::answer(const Message& request, Kind kind, std::vector<std::byte> payload) {
  return !broken_ && request.kind == Kind::Request &&
         outbox_.push(
             {.lane = request.lane,
              .kind = kind,
              .channel = request.channel,
              .request = request.request,
              .payload = std::move(payload)}
         );
}

bool Endpoint::reply(const Message& request, std::vector<std::byte> payload) {
  return answer(request, Kind::Reply, std::move(payload));
}

bool Endpoint::fail(const Message& request, std::vector<std::byte> payload) {
  return answer(request, Kind::Error, std::move(payload));
}

// The cancel rides the request's lane so it never overtakes the request.
void Endpoint::cancel(const std::vector<Requests::Ended>& ended) {
  if (broken_) {
    return;
  }
  for (const auto [id, lane] : ended) {
    if (!outbox_.withdraw(lane, id)) {
      outbox_.push({.lane = lane, .kind = Kind::Cancel, .request = id, .payload = {}});
    }
  }
}

void Endpoint::release(Owner& owner) {
  std::erase(owners_, &owner);
  owner.endpoint_ = nullptr;
  cancel(requests_.drop(&owner));
}

void Endpoint::poll() {
  Sink sink(*this);
  while (!broken_) {
    const auto io = transport_.receive(frame_);
    if (io == Io::Again) {
      return;
    }
    if (io == Io::Corrupt || !inbox_.accept(frame_, sink)) {
      breakDown();
    }
  }
}

void Endpoint::flush() {
  if (!broken_ && outbox_.flush(transport_) == Io::Corrupt) {
    breakDown();
  }
}

void Endpoint::tick(std::uint64_t now) {
  now_ = now;
  cancel(requests_.expire(now));
}

void Endpoint::breakDown() {
  broken_ = true;
  inbox_.reset();
  outbox_.reset();
  requests_.failAll();
}

void Endpoint::reset() {
  inbox_.reset();
  outbox_.reset();
  broken_ = false;
  requests_.failAll();
}

}  // namespace glim::transport
