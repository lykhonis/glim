#include <glim/toolkit.h>
#include <glim/toolkit/host/Codec.h>
#include <glim/toolkit/host/Host.h>
#include <glim/toolkit/host/SeatView.h>

#include <cassert>
#include <iostream>
#include <typeinfo>
#include <vector>

int main() {
  using glim::shell::Key;
  using glim::toolkit::host::mapKey;

  assert(mapKey(Key::DpadLeft) == glim::toolkit::Key::Left);
  assert(mapKey(Key::DpadRight) == glim::toolkit::Key::Right);
  assert(mapKey(Key::DpadUp) == glim::toolkit::Key::Up);
  assert(mapKey(Key::DpadDown) == glim::toolkit::Key::Down);
  assert(mapKey(Key::Enter) == glim::toolkit::Key::Activate);
  assert(mapKey(Key::DpadCenter) == glim::toolkit::Key::Activate);
  assert(!mapKey(Key::Escape));
  assert(!mapKey(Key::Unknown));

  // Seat factory: id 0 never creates a view.
  using glim::toolkit::host::makeDefaultSeatView;
  using glim::toolkit::host::SeatRequest;
  assert(makeDefaultSeatView(SeatRequest{}) == nullptr);
  SeatRequest named{7, glim::Rect{{10, 20}, {100, 40}}};
  assert(named.id == 7 && named.bounds.size.x == 100);
  // Factory ownership: same id returns the same live view (Glim never
  // retains; the factory does).
  void* first = makeDefaultSeatView(named);
  void* second = makeDefaultSeatView(named);
#if defined(__APPLE__)
  assert(first != nullptr && first == second);
#else
  assert(first == nullptr && second == nullptr);
#endif

  // Primitive ops: elements serialize without new codec edits.
  {
    using glim::toolkit::Box;
    using glim::toolkit::Op;
    using glim::toolkit::OpsRecorder;
    OpsRecorder rec;
    rec.fill({0, 0, 10, 10}, 0xff0000ff, "m1");
    rec.fillRounded({0, 0, 10, 10}, 3, 0x00ff00ff);
    rec.fillGradient({0, 0, 10, 10}, 7, 0x111111ff, 0x222222ff, "m2");
    rec.stroke({0, 0, 10, 10}, 4, 2.0f, 0xffffffff);
    rec.text(1.0f, 2.0f, "hi", 0xffffffff, 14.0f, "m3");
    rec.image({0, 0, 10, 10}, 9, true);
    rec.pushGlass(
        {0, 0, 10, 10}, 12, true, static_cast<std::uint8_t>(glim::toolkit::GlassEdge::Full), 0
    );
    rec.pushClip({0, 0, 10, 10});
    rec.pop();
    rec.slot({0, 0, 10, 10}, 7);
    std::vector<std::uint8_t> bytes;
    glim::toolkit::host::encodeOps(rec.ops, bytes);
    std::vector<Op> back;
    assert(glim::toolkit::host::decodeOps(bytes.data(), bytes.size(), back));
    assert(back == rec.ops);
    assert(!glim::toolkit::host::decodeOps(bytes.data(), bytes.size() - 1, back));
    bytes[3] ^= 0xff;
    assert(!glim::toolkit::host::decodeOps(bytes.data(), bytes.size(), back));
  }

  // Theme defaults mirror the fallback; a themed scope overrides them.
  {
    using glim::toolkit::Engine;
    using glim::toolkit::themeColor;
    using glim::toolkit::themed;
    constexpr static glim::toolkit::ThemeKey kTestCard{glim::toolkit::themeKey("test.card")};
    constexpr std::uint32_t kTestCardDefault = 0x2c2c2eff;
    glim::toolkit::Env env{nullptr, nullptr, std::nullopt};
    Engine plain{env, glim::toolkit::StoreRef{}};
    assert(themeColor(plain, kTestCard, kTestCardDefault) == kTestCardDefault);
    struct Probe {
      const char* kind() const { return "Probe"; }
      std::uint32_t& out;
      glim::toolkit::Box place(glim::toolkit::Constraints c, const Engine& e) const {
        out = themeColor(e, kTestCard, kTestCardDefault);
        return glim::toolkit::detail::resolveSize(10, 10, c);
      }
    };
    std::uint32_t seen = 0;
    auto tree = themed(
        themeColor(kTestCard, 0x123456ff), glim::toolkit::Block{glim::toolkit::Custom{Probe{seen}}}
    );
    glim::toolkit::place(tree, {0, 720, 0, 480});
    assert(seen == 0x123456ff);
  }

  std::cout << "host_test ok\n";
  return EXIT_SUCCESS;
}
