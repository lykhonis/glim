#include <glim/paint/Software.h>
#include <glim/toolkit.h>
#include <glim/toolkit/host/Codec.h>
#include <glim/toolkit/host/Crash.h>
#include <glim/toolkit/host/Drive.h>
#include <glim/toolkit/host/Host.h>
#include <glim/toolkit/host/Paint.h>
#include <glim/toolkit/host/Power.h>
#include <glim/toolkit/host/Probe.h>
#include <glim/toolkit/host/SeatView.h>
#include <glim/toolkit/host/Startup.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

namespace {

// Custom element that paints its own effects: a shadow whose caster is
// hidden (opacity 0) and an additive glow, both through the AnySink.
struct FxCard {
  static constexpr const char* kKind = "FxCard";
  glim::toolkit::Box place(glim::toolkit::Constraints c, const glim::toolkit::Engine&) const {
    return glim::toolkit::detail::resolveSize(60.f, 40.f, c);
  }
  void record(const glim::toolkit::Box& box, glim::toolkit::Rec& rec) const {
    rec.push(kKind, box);
  }
  void
  paint(const glim::toolkit::Box& b, glim::toolkit::AnySink& sink, const glim::toolkit::Engine&)
      const {
    sink.pushShadow(6.f, 8.f, 8.f, 0x000000ffu, 0.f, 0.f);
    sink.fillRounded(b, 4, 0xff0000ffu);
    sink.pop();
    sink.pushEffect(
        b, 10, static_cast<std::uint32_t>(glim::toolkit::EffectKind::Glow), 0x00ff00ffu, 1.f
    );
    sink.pop();
  }
};

glim::toolkit::Block fxScene() {
  using namespace glim::toolkit;
  return column(
      gap(0),
      pad(40),
      column(
          shadow(6.f, 0.f, 0.f, 0x000000ffu, 0.f, 0.5f),
          effectGlow(0x0000ffffu, 1.f, 4),
          finish(FxCard{})
      )
  );
}

}  // namespace

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

  // Every newer op kind survives the archive round trip field for field.
  {
    using glim::toolkit::Op;
    using glim::toolkit::OpsRecorder;
    OpsRecorder rec;
    const float pts[6] = {0, 0, 5, 5, 10, 0};
    rec.pushTransform(3.0f, -4.0f);
    rec.pushRotate(30.0f, 5.0f, 6.0f);
    rec.fillRadial({0, 0, 10, 10}, 2, 0xff0000ff, 0xff000000, "r");
    rec.fillChamfer({0, 0, 10, 10}, 3.0f, 0x00ff00ff);
    rec.strokeChamfer({0, 0, 10, 10}, 3.0f, 1.5f, 0x0000ffff, "c");
    rec.arc(5, 5, 4, 1, -90, 180, 0xffffffff);
    rec.polyline(pts, 3, 2.0f, 6.0f, 0x123456ff, "p");
    rec.pushShadow(12.0f, 1.0f, 4.0f, 0x000000a6, 2.0f, 0.25f);
    rec.pushEffect({0, 0, 10, 10}, 5, 0, 0xff6a00ff, 0.3f);
    for (int i = 0; i < 4; ++i) rec.pop();
    std::vector<std::uint8_t> bytes;
    glim::toolkit::host::encodeOps(rec.ops, bytes);
    std::vector<Op> back;
    assert(glim::toolkit::host::decodeOps(bytes.data(), bytes.size(), back));
    assert(back == rec.ops);
  }

  // Lens material survives the op archive round trip.
  {
    using glim::toolkit::Box;
    using glim::toolkit::Op;
    using glim::toolkit::OpsRecorder;
    OpsRecorder rec;
    rec.pushGlass({0, 0, 10, 10}, 12, false, 15, 0xd8a84eff, 1);
    rec.pushGlass({0, 0, 10, 10}, 8, false, 15, 0, 2);
    std::vector<std::uint8_t> bytes;
    glim::toolkit::host::encodeOps(rec.ops, bytes);
    std::vector<Op> back;
    assert(glim::toolkit::host::decodeOps(bytes.data(), bytes.size(), back));
    assert(back == rec.ops);
    assert(back[0].material == 1 && back[0].tint == 0xd8a84eff);
    assert(back[1].material == 2);
  }

  // Pre-v4 archives carry no material byte: decode as refractive glass.
  {
    using glim::toolkit::Op;
    std::vector<std::uint8_t> v3;
    const char* magic = "GLIMUI02";
    for (int i = 0; i < 8; ++i) v3.push_back(static_cast<std::uint8_t>(magic[i]));
    v3.push_back(3);
    v3.push_back(0);  // u16 version 3, little-endian
    v3.push_back(1);
    v3.push_back(0);
    v3.push_back(0);
    v3.push_back(0);  // u32 op count 1
    v3.push_back(static_cast<std::uint8_t>(Op::Kind::PushGlass));
    auto putF32 = [&](float f) {
      std::uint32_t u = 0;
      std::memcpy(&u, &f, sizeof(u));
      for (int i = 0; i < 4; ++i) v3.push_back(static_cast<std::uint8_t>((u >> (8 * i)) & 0xff));
    };
    putF32(0.f);
    putF32(0.f);
    putF32(10.f);
    putF32(10.f);  // box
    v3.push_back(0);
    v3.push_back(0);  // empty mark
    v3.push_back(12);
    v3.push_back(0);
    v3.push_back(0);
    v3.push_back(0);   // i32 radius 12
    v3.push_back(1);   // clear
    v3.push_back(15);  // edge
    v3.push_back(0);
    v3.push_back(0);
    v3.push_back(0);
    v3.push_back(0);  // u32 tint 0
    std::vector<Op> back;
    assert(glim::toolkit::host::decodeOps(v3.data(), v3.size(), back));
    assert(back.size() == 1);
    assert(back[0].kind == Op::Kind::PushGlass);
    assert(back[0].material == 0);
    assert(back[0].clear && back[0].edge == 15 && back[0].radius == 12);
  }

  // Effects mix: tags on a composed node and effects a custom element
  // paints itself nest in one op stream, survive the archive, and render.
  {
    using namespace glim::toolkit;
    host::LayerFeed feed;
    Block t = fxScene();
    const Box tb = place(t, {0, 200, 0, 200});
    OpsRecorder rec;
    paintTree(t, tb, Engine{}, rec, feed);
    std::vector<Op::Kind> pushes;
    std::vector<float> casters;
    int depth = 0, maxDepth = 0;
    for (const Op& o : rec.ops) {
      if (o.kind == Op::Kind::PushShadow || o.kind == Op::Kind::PushEffect) {
        pushes.push_back(o.kind);
        maxDepth = std::max(maxDepth, ++depth);
        if (o.kind == Op::Kind::PushShadow) casters.push_back(o.casterOpacity);
      }
      if (o.kind == Op::Kind::Pop) --depth;
    }
    // Tag shadow, tag effect, then the card's own shadow and glow inside them.
    assert(
        (pushes ==
         std::vector<Op::Kind>{
             Op::Kind::PushShadow, Op::Kind::PushEffect, Op::Kind::PushShadow, Op::Kind::PushEffect
         })
    );
    assert((casters == std::vector<float>{0.5f, 0.f}));
    assert(maxDepth == 3 && depth == 0);
    std::vector<std::uint8_t> bytes;
    host::encodeOps(rec.ops, bytes);
    std::vector<Op> back;
    assert(host::decodeOps(bytes.data(), bytes.size(), back) && back == rec.ops);

    // Same tree through the paint pipeline (CPU raster).
    glim::paint::Context ctx;
    ctx.setSize({200, 200});
    ctx.beginFrame();
    ctx.setFillColor(0x404040ffu);
    ctx.fill(glim::Rect::fromSize({200, 200}));
    host::ContextSink sink{ctx};
    paintTree(t, tb, Engine{}, sink, feed);
    ctx.finish();
    std::vector<std::uint8_t> px(200 * 200 * 4);
    glim::paint::rasterScene(ctx.scene(), 200, 200, px.data());
    auto at = [&](int x, int y) { return &px[static_cast<std::size_t>((y * 200 + x) * 4)]; };
    // Card at (40,40,60,40) on gray: the hidden caster paints no red, the
    // glow adds green, and the card's own shadow darkens below-right.
    const std::uint8_t bg = at(5, 5)[0];
    const std::uint8_t* c = at(70, 60);
    assert(c[0] < bg && c[1] > 150);
    const std::uint8_t* s = at(104, 84);
    assert(s[0] < bg && s[1] < bg && s[2] < bg);
  }

  // pushGlass material maps to the matching paint lens preset.
  {
    using glim::toolkit::GlassMaterial;
    using glim::toolkit::host::ContextSink;
    auto painted = [](std::uint8_t material, std::uint32_t tint) {
      glim::paint::Context ctx;
      ctx.setSize({100, 100});
      ctx.beginFrame();
      ContextSink sink{ctx};
      sink.pushGlass({0, 0, 50, 50}, 12, false, 15, tint, material);
      sink.pop();
      ctx.finish();
      const glim::paint::Scene& scene = ctx.scene();
      assert(!scene.root.children.empty());
      const auto& params = scene.root.children[0]->params;
      assert(params.lens.has_value());
      return *params.lens;
    };
    const glim::paint::Lens def = painted(0, 0);
    assert(def.material == glim::paint::LensMaterial::Glass);
    const glim::paint::Lens metal =
        painted(static_cast<std::uint8_t>(GlassMaterial::LiquidMetal), 0xd8a84eff);
    assert(metal.material == glim::paint::LensMaterial::LiquidMetal);
    assert(metal.transmit == 0.f && metal.tint.rgba == 0xd8a84eff);
    const glim::paint::Lens water = painted(static_cast<std::uint8_t>(GlassMaterial::Water), 0);
    assert(water.material == glim::paint::LensMaterial::Water);
    assert(water.transmit == 1.f);
    const glim::paint::Lens teal =
        painted(static_cast<std::uint8_t>(GlassMaterial::Water), 0x2a9d8fff);
    assert(teal.tint.rgba == 0x2a9d8fff);
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

  // Probe: one command surface; every answer ends in ok/err.
  {
    namespace host = glim::toolkit::host;
    host::TestApp app;
    app.w = 96;
    app.h = 32;
    app.build = [](const glim::toolkit::Engine&) { return glim::toolkit::lines("Probe"); };
    const std::string dir =
        (std::filesystem::temp_directory_path() / "glim-host-test-probe").string();
    std::ostringstream out;
    host::Probe probe(app, out);
    assert(probe.exec("out " + dir));
    assert(probe.exec("snap " + dir + "-ref.png cpu"));
    assert(probe.exec("ref " + dir + "-ref.png"));
    assert(probe.exec("region all 0 0 96 32"));
    assert(probe.exec("compare self"));
    const std::string s = out.str();
    assert(s.find("region all mad=0.00") != std::string::npos);
    assert(s.find("over2=0") != std::string::npos);
    assert(s.find("ok\n") != std::string::npos);
    assert(probe.exec("hit 2 2"));
    assert(probe.exec("expect Probe"));
    assert(!probe.exec("expect Missing"));
    assert(!probe.exec("no-such-command"));
    assert(out.str().rfind("err\n") == out.str().size() - 4);
    assert(probe.exec("encode"));
    assert(out.str().find("shape GlyphRun count=") != std::string::npos);
    assert(probe.exec("bbox cur 0 0 96 32 bright"));
    assert(probe.exec("prof ref row 16"));
    assert(host::probeLineFromE2e("tap:i:1") == "tap i 1");
    assert(host::probeLineFromE2e("wheel:1,2,3,4") == "wheel 1 2 3 4");
    assert(host::probeLineFromE2e("~FRAME") == "expect FRAME");
    assert(host::probeLineFromE2e("snap:/a,b.png") == "snap /a,b.png");

    // Measurement commands answer about the image they name.
    const auto said = [&](const std::string& cmd, const std::string& want) {
      const std::size_t from = out.str().size();
      const bool ok = probe.exec(cmd);
      return ok && out.str().find(want, from) != std::string::npos;
    };
    assert(said("render", "ok"));
    assert(said("pick cur 0 0", "pick #"));
    assert(!probe.exec("pick cur 500 0"));
    assert(said("color cur 0 0 96 32", "color ink="));
    assert(said("item word 0 0 96 32 bright", "ok"));
    assert(said("items", "items matched=1 mean_abs_px=0.00"));
    assert(said("zoom 0 0 32 16 2 8", "mad=0.00"));
    assert(said("ruler ref 0 0 32 16 2", "ruler "));
    assert(said("prof cur row 16 0 95 raw", "peaks"));
    assert(said("runs cur row 16 0 95 bright", "runs"));
    assert(!probe.exec("runs cur row 16 0 95"));
    assert(said("ray cur 48 16 0 0 20", "ray"));
    assert(said("ring cur 48 16 10 bright", "ring"));
    assert(said("bbox cur 0 0 1 1 dim", "bbox"));
    assert(!probe.exec("bbox nope 0 0 1 1 dim"));
    assert(said("tree", "Probe"));
    assert(probe.exec("a11y"));
    assert(said("stats", "ops"));
    assert(said("ops", "ops total="));
    assert(probe.exec("tops"));
    assert(said("bench 2", "bench frames=2"));
    assert(said("hit 2 2", "box="));
    assert(probe.exec("tap 2 2") && probe.exec("tap i 0") && probe.exec("key Next"));
    assert(!probe.exec("key NoSuchKey"));
    assert(probe.exec("wheel 10 10 0 -5") && probe.exec("step 0.1"));
    assert(said("size 120 40", "ok"));
    assert(said("history", "ok"));
    assert(said("help", "ok"));
    assert(probe.exec("# comment") && probe.exec(""));
    std::istringstream script("render\nbad-command\nrender\nquit\nrender\n");
    assert(!probe.run(script, false));

    host::Probe bare(app, out);
    assert(!bare.exec("items") && !bare.exec("zoom 0 0 8 8"));
    assert(!bare.exec("ref " + dir + "/missing.png"));
    assert(!bare.exec("item x 0 0 8 8 nomask"));
    std::filesystem::remove_all(dir);
    std::filesystem::remove(dir + "-ref.png");
  }

  // Headless driver: every flag writes what it promises; soak passes
  // on a static tree.
  {
    namespace host = glim::toolkit::host;
    namespace fs = std::filesystem;
    host::TestApp app;
    app.w = 96;
    app.h = 32;
    app.build = [](const glim::toolkit::Engine&) { return glim::toolkit::lines("Driver"); };
    const fs::path dir = fs::temp_directory_path() / "glim-host-test-e2e";
    fs::create_directories(dir);
    const auto flagsOf = [](std::vector<std::string> args) {
      args.insert(args.begin(), "app");
      std::vector<char*> argv;
      for (std::string& a : args) argv.push_back(a.data());
      return host::parseE2e(static_cast<int>(argv.size()), argv.data());
    };
    const host::E2eFlags f = flagsOf(
        {"--e2e=step:0.1;~Driver",
         "--soak",
         "3",
         "--trace-frames=4",
         "--trace-out",
         (dir / "trace.csv").string(),
         "--trace-json=" + (dir / "trace.json").string(),
         "--snapshot",
         (dir / "snap.png").string(),
         "--dump-ops",
         (dir / "ops.bin").string(),
         "--drive=d.drive",
         "--drive-trace=d.json"}
    );
    assert(f.any() && f.soak == 3 && f.traceFrames == 4);
    assert(f.drive == "d.drive" && f.driveTrace == "d.json");
    assert(host::runE2e(app, f) == 0);
    for (const char* name : {"trace.csv", "trace.json", "snap.png", "ops.bin"})
      assert(fs::file_size(dir / name) > 0);
    std::ifstream csv(dir / "trace.csv");
    std::string header;
    std::getline(csv, header);
    assert(header == "frame,buildMs,recordMs,encodeMs,ops,rev");
    assert(flagsOf({"--soak"}).soak == 60 && flagsOf({"--trace-frames"}).traceFrames == 60);
    assert(flagsOf({"-h"}).help && !flagsOf({"--startup"}).any());
    assert(host::runE2e(app, flagsOf({"--e2e", "~Missing"})) == 1);
    assert(host::runE2e(app, flagsOf({"--probe", (dir / "none.probe").string()})) == 1);
    { std::ofstream(dir / "ok.probe") << "# probe\nexpect Driver\n"; }
    assert(host::runE2e(app, flagsOf({"--probe", (dir / "ok.probe").string()})) == 0);
    assert(host::runE2e(app, flagsOf({"--trace-frames=1", "--trace-out=/no/such/dir/x.csv"})) == 1);
    fs::remove_all(dir);
  }

  // Drive scripts: a timeline of shell events; any bad line rejects
  // the whole script.
  {
    namespace host = glim::toolkit::host;
    using glim::shell::EventType;
    const host::DriveScript s = host::parseDriveScript(
        "# warm up\n"
        "wait 1\n"
        "click 10 20 2 0.2   # two taps\n"
        "drag 0 0 100 50 0.5 10\n"
        "key right\n"
    );
    assert(s.error.empty());
    // 2 clicks (down+up) + drag (down, 5 moves, up) + key.
    assert(s.steps.size() == 4 + 7 + 1);
    assert(s.steps[0].event.type() == EventType::PointerDown && s.steps[0].at == 1.0f);
    assert(s.steps[0].event.x() == 10.0f && s.steps[0].event.y() == 20.0f);
    assert(s.steps[1].event.type() == EventType::PointerUp && s.steps[1].at > 1.0f);
    assert(std::fabs(s.steps[2].at - 1.2f) < 1e-5f);
    const auto& dragEnd = s.steps[10];
    assert(dragEnd.event.type() == EventType::PointerUp && dragEnd.event.x() == 100.0f);
    assert(std::fabs(dragEnd.at - 1.9f) < 1e-5f);
    assert(s.steps[9].event.type() == EventType::PointerMove && s.steps[9].event.y() == 50.0f);
    assert(
        s.steps[11].event.type() == EventType::KeyDown && s.steps[11].event.key() == Key::DpadRight
    );
    for (std::size_t i = 1; i < s.steps.size(); ++i) assert(s.steps[i].at >= s.steps[i - 1].at);
    assert(!host::parseDriveScript("click 1\n").error.empty());
    assert(!host::parseDriveScript("wait 1\nfly 2\n").error.empty());
    assert(host::parseDriveScript("wait 1\nfly 2\n").steps.empty());
    assert(!host::parseDriveScript("key warp\n").error.empty());

    // Report: percentiles over what was recorded; a long wait or a
    // frozen frame is a stall.
    host::DriveReport r;
    host::FrameTelemetry t;
    t.frameMs = 16.7f;
    t.buildMs = 1.0f;
    t.recordMs = 1.0f;
    t.encodeMs = 3.0f;
    t.stats.acquireMs = 2.0f;
    t.stats.gpuMs = 4.0f;
    for (int i = 0; i < 10; ++i) {
      r.event(i * 10.0f, EventType::PointerDown, 1.0f, 0.5f);
      r.paint(i * 16.7f, t);
    }
    assert(!r.stalled());
    const std::string sum = r.summary();
    assert(sum.find("10 events, 10 paints") != std::string::npos);
    assert(sum.find("0 events lagged, 0 frame gaps") != std::string::npos);
    assert(sum.find("  gpu      p50    4.00") != std::string::npos);
    assert(r.traceJson().find("\"name\":\"down\"") != std::string::npos);
    host::DriveReport kinds;
    for (EventType k :
         {EventType::PointerMove, EventType::PointerUp, EventType::KeyDown, EventType::Frame})
      kinds.event(0.0f, k, 0.0f, 0.0f);
    const std::string kindsJson = kinds.traceJson();
    for (const char* name : {"move", "up", "key", "other"})
      assert(kindsJson.find(std::string("\"name\":\"") + name + "\"") != std::string::npos);

    // Loading: a missing or bad script yields no run; a good one finishes
    // and writes its trace.
    namespace fs = std::filesystem;
    const fs::path drive = fs::temp_directory_path() / "glim-host-test.drive";
    const fs::path trace = fs::temp_directory_path() / "glim-host-test-drive.json";
    assert(!host::DriveRun::load((fs::temp_directory_path() / "no-such.drive").c_str()));
    std::ofstream(drive) << "fly 1\n";
    assert(!host::DriveRun::load(drive.c_str()));
    std::ofstream(drive) << "wait 0.1\n";
    const std::unique_ptr<host::DriveRun> run = host::DriveRun::load(drive.c_str());
    assert(run);
    run->paint(t);
    assert(run->finish(trace.c_str()) && fs::file_size(trace) > 0);
    fs::remove(drive);
    fs::remove(trace);
    host::DriveReport late = r;
    late.event(200.0f, EventType::PointerUp, 450.0f, 0.5f);
    assert(late.stalled() && late.summary().find("1 events lagged") != std::string::npos);
    host::DriveReport frozen = r;
    t.frameMs = 1000.0f;
    frozen.paint(1200.0f, t);
    assert(frozen.stalled() && frozen.summary().find("1 frame gaps") != std::string::npos);
  }

  {
    namespace host = glim::toolkit::host;
    host::StartupReport r;
    r.markAt("launch", 10.0);
    r.markAt("record", 35.5);
    r.markAt("presented", 40.0);
    assert(r.totalMs() == 40.0);
    assert(
        r.summary() ==
        "startup launch           10.0\n"
        "startup record           25.5\n"
        "startup presented         4.5\n"
        "startup total            40.0\n"
    );
    assert(host::processUptimeMs() > 0.0);

    auto startupOf = [](const char* flag) {
      char app[] = "app";
      std::string arg = flag;
      char* argv[] = {app, arg.data()};
      return host::parseE2e(2, argv).startup;
    };
    assert(startupOf("--startup") == host::StartupUntil::Presented);
    assert(startupOf("--startup-encoded") == host::StartupUntil::Encoded);
    assert(startupOf("--stats") == host::StartupUntil::Off);
  }

  {
    namespace host = glim::toolkit::host;
    const auto admitted = [](float fps) {
      host::FramePacer pacer;
      int count = 0;
      for (int i = 0; i < 60; ++i) {
        count += pacer.admit(i / 60.0 + (i % 2 ? 0.0004 : -0.0004), fps) ? 1 : 0;
      }
      return count;
    };
    assert(admitted(0.f) == 60);
    assert(admitted(60.f) == 60);
    assert(admitted(30.f) == 30);
    assert(admitted(20.f) == 20);

    host::PowerPolicy policy;
    assert(host::targetFps(policy) == 0.f);
    bool low = false;
    policy.lowPower = [&low] { return low; };
    assert(host::targetFps(policy) == 0.f);
    low = true;
    assert(host::targetFps(policy) == 30.f);
    policy.maxFps = 24.f;
    assert(host::targetFps(policy) == 24.f);
    policy.maxFps = 50.f;
    assert(host::targetFps(policy) == 30.f);
    low = false;
    assert(host::targetFps(policy) == 50.f);
  }

  {
    namespace host = glim::toolkit::host;
    const std::string path =
        (std::filesystem::temp_directory_path() / "glim-host-test.crash").string();
    std::filesystem::remove(path);
    assert(!host::takeCrashReport(path));

    const auto crashChild = [&](int signal, bool chained) {
      const pid_t pid = fork();
      if (pid == 0) {
        if (chained) {
          ::signal(signal, [](int) { std::exit(42); });
        }
        if (!host::installCrashHandler(path)) _exit(3);
        raise(signal);
        _exit(4);
      }
      int status = 0;
      waitpid(pid, &status, 0);
      return status;
    };

    const int killed = crashChild(SIGSEGV, false);
    assert(WIFSIGNALED(killed) && WTERMSIG(killed) == SIGSEGV);
    const auto report = host::takeCrashReport(path);
    assert(report && report->rfind("glim crash\nsignal 11\n#0 0x", 0) == 0);
    assert(report->find("glim-toolkit-host-test +0x") != std::string::npos);
    assert(!host::takeCrashReport(path));

    const int chained = crashChild(SIGABRT, true);
    assert(WIFEXITED(chained) && WEXITSTATUS(chained) == 42);
    const auto aborted = host::takeCrashReport(path);
    assert(aborted && aborted->find("signal 6\n") != std::string::npos);
  }

  std::cout << "host_test ok\n";
  return EXIT_SUCCESS;
}
