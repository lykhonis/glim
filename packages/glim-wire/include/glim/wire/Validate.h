#pragma once

#include <glim/wire/Record.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace glim::wire {

enum class Status : std::uint8_t {
  Ok,
  Truncated,
  BadSize,
  BadFixed,
  BadTail,
  MustUnderstand,
  Rejected,
};

const char* statusName(Status status);

struct TailRule {
  std::uint32_t refOffset;
  std::uint32_t elementSize;
};

// Per-type contract for the reader. `check` validates field values (enum
// ranges, float sanity) on a record whose framing and tails already passed.
struct TypeRule {
  std::uint16_t type;
  std::span<const TailRule> tails;
  bool (*check)(RecordView record) = nullptr;
};

// The one edge validator (K73): checks every record in `message` against
// the framing rules and, for known types, their tail and value rules.
// `message` must start 8-aligned relative to the record stream.
Status validate(std::span<const std::byte> message, std::span<const TypeRule> rules);

const TypeRule* findRule(std::span<const TypeRule> rules, std::uint16_t type);

// Visits records of known types in a message that passed `validate`;
// unknown records are skipped by size.
template <class Visit>
void forEachKnown(
    std::span<const std::byte> message, std::span<const TypeRule> rules, Visit&& visit
) {
  for (std::size_t at = 0; at < message.size();) {
    RecordView record(message.data() + at);
    if (findRule(rules, record.type())) {
      visit(record);
    }
    at += record.size();
  }
}

}  // namespace glim::wire
