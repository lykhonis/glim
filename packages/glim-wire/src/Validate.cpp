#include <glim/wire/Validate.h>

namespace glim::wire {

namespace {

Status checkKnown(RecordView record, const TypeRule& rule) {
  for (const TailRule& t : rule.tails) {
    const auto tail = record.tail(t.refOffset);
    if (!tail || (t.elementSize > 1 && tail->size() % t.elementSize != 0)) {
      return Status::BadTail;
    }
  }
  if (rule.check && !rule.check(record)) {
    return Status::Rejected;
  }
  return Status::Ok;
}

Status checkRecord(std::span<const std::byte> rest, std::span<const TypeRule> rules) {
  if (rest.size() < kHeaderSize) {
    return Status::Truncated;
  }
  RecordView record(rest.data());
  const auto size = record.size();
  if (size < kHeaderSize || size % kAlign != 0) {
    return Status::BadSize;
  }
  if (size > rest.size()) {
    return Status::Truncated;
  }
  if (record.fixedSize() < kHeaderSize || record.fixedSize() > size) {
    return Status::BadFixed;
  }
  if (const TypeRule* rule = findRule(rules, record.type())) {
    return checkKnown(record, *rule);
  }
  return (record.flags() & kMustUnderstand) ? Status::MustUnderstand : Status::Ok;
}

}  // namespace

const char* statusName(Status status) {
  switch (status) {
    case Status::Ok:
      return "ok";
    case Status::Truncated:
      return "truncated";
    case Status::BadSize:
      return "bad size";
    case Status::BadFixed:
      return "bad fixed size";
    case Status::BadTail:
      return "bad tail";
    case Status::MustUnderstand:
      return "must understand";
    case Status::Rejected:
      return "rejected";
  }
  return "unknown";
}

const TypeRule* findRule(std::span<const TypeRule> rules, std::uint16_t type) {
  for (const TypeRule& rule : rules) {
    if (rule.type == type) {
      return &rule;
    }
  }
  return nullptr;
}

Status validate(std::span<const std::byte> message, std::span<const TypeRule> rules) {
  for (std::size_t at = 0; at < message.size();) {
    const auto rest = message.subspan(at);
    if (const Status status = checkRecord(rest, rules); status != Status::Ok) {
      return status;
    }
    at += RecordView(rest.data()).size();
  }
  return Status::Ok;
}

}  // namespace glim::wire
