#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Showcase.h"
#include "glim/toolkit.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/Paint.h"
#include "glim/toolkit/host/Telemetry.h"
#include "widgets/AlertCard.h"
#include "widgets/Button.h"
#include "widgets/Card.h"
#include "widgets/Checkbox.h"
#include "widgets/Field.h"
#include "widgets/Header.h"
#include "widgets/Kpi.h"
#include "widgets/Nav.h"
#include "widgets/Progress.h"
#include "widgets/Segmented.h"
#include "widgets/Sidebar.h"
#include "widgets/Slider.h"
#include "widgets/StageRow.h"
#include "widgets/StripChart.h"
#include "widgets/Strut.h"
#include "widgets/Switch.h"
#include "widgets/TelemetryPanel.h"
#include "widgets/TickRing.h"
#include "widgets/Type.h"
#include "widgets/Wide.h"

using namespace glim::toolkit;

namespace glim {
namespace toolkit {

float ringValue(const host::TelemetryHub* hub) {
  if (hub != nullptr && !hub->empty()) {
    float v = hub->last().frameMs / 33.0f;
    if (v < 0.f) return 0.f;
    if (v > 1.f) return 1.f;
    return v;
  }
  return 0.45f;
}

namespace {

constexpr float kRailWidth = 232.0f;
// Floating glass header: matches Host titleDragHeight so the strip
// doubles as the window drag zone. Dark plate tint for the wash.
constexpr float kHeaderH = 52.0f;
constexpr std::uint32_t kHeaderTintDark = 0x141417a6u;

// Breakdown: stat unit-style stacked stage bars (frame/build/record/
// encode) from the latest sample. Box-driven so Wide stretches it.
struct Breakdown {
  static constexpr const char* kKind = "Breakdown";
  float frameMs = 0.f;
  float buildMs = 0.f;
  float recordMs = 0.f;
  float encodeMs = 0.f;
  Box place(Constraints c, const Engine&) const {
    float w = 240.0f;
    if (!isUnbounded(c.maxW)) w = c.maxW;
    return detail::resolveSize(w, 4 * 20.0f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 4);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const char* names[4] = {"frame", "build", "record", "encode"};
    float vals[4] = {frameMs, buildMs, recordMs, encodeMs};
    const float warn[4] = {20.0f, 8.0f, 8.0f, 8.0f};
    const std::uint32_t ink =
        themeColor(e, kThemeInkOnLight, Tokens::kInkOnLight);
    const float size = themeFloat(e, kThemeTypeLine, Tokens::kTypeLine);
    for (int i = 0; i < 4; ++i) {
      const float y = box.y + static_cast<float>(i) * 20.0f + 3.0f;
      sink.text(box.x, y, names[i], ink, size);
      // Value first (measured, right-docked), then the bar spans
      // label → value with even gutters on both sides.
      char ms[32];
      std::snprintf(
          ms, sizeof(ms), "%.1f", static_cast<double>(vals[i])
      );
      Box mw = text::measureText(ms, {0, kUnbounded, 0, kUnbounded}, e);
      const float vx = box.x + box.w - mw.w;
      sink.text(vx, y, ms, ink, size);
      const float bx = box.x + 64.0f;
      const float bw = vx - 8.0f - bx;
      Box bg{bx, y, bw > 0 ? bw : 0, 8.0f};
      sink.fillRounded(bg, 4, withAlpha(ink, 0.08f));
      float t = vals[i] / 40.0f;
      if (t < 0.f) t = 0.f;
      if (t > 1.f) t = 1.f;
      if (t > 0.01f) {
        Box fg{bx, y, bg.w * t, 8.0f};
        bool hot = vals[i] >= warn[i];
        // HUD bars: monochrome fill, red past the warn threshold.
        sink.fillRounded(
            fg,
            4,
            hot ? themeColor(e, kShowBad, Show::kBad)
                : withAlpha(ink, 0.55f)
        );
      }
    }
  }
};

template <class... As>
auto breakdown(Breakdown b, As&&... as) {
  configureTags(b, std::forward<As>(as)...);
  return finish(std::move(b), std::forward<As>(as)...);
}

Block breakdownFromHub(const host::TelemetryHub* hub) {
  host::FrameTelemetry s{};
  if (hub != nullptr && !hub->empty()) s = hub->last();
  return breakdown(
      {.frameMs = s.frameMs,
       .buildMs = s.buildMs,
       .recordMs = s.recordMs,
       .encodeMs = s.encodeMs}
  );
}

Block alertFeed(const std::vector<std::string>* log) {
  std::vector<Block> kids;
  kids.push_back(lines("// ACTIVITY"));
  if (log == nullptr || log->empty()) {
    kids.push_back(alertCard(
        {.title = "No events yet",
         .detail = "push log is empty",
         .severity = 0}
    ));
  } else {
    for (AlertCard& a : alertCards(*log))
      kids.push_back(alertCard(std::move(a)));
  }
  Column col;
  col.gap = 8;
  col.children = std::move(kids);
  return toBlock(std::move(col));
}

// Overview: one live-telemetry section. Every number comes from
// the hub (pull) or the classified push log — no synthetic filler.
// Null hub renders deterministic placeholders for headless runs.
Block overviewPage(
    Scope,
    const host::TelemetryHub* hub,
    const std::vector<std::string>* log,
    State<bool>& live
) {
  const bool has = hub != nullptr && !hub->empty();
  char frameV[64], rateV[64], drawsV[64], hitchV[64], heroTag[64];
  if (has) {
    const host::FrameTelemetry s = hub->last();
    std::snprintf(
        frameV, sizeof(frameV), "%.1f", static_cast<double>(s.frameMs)
    );
    std::snprintf(
        heroTag,
        sizeof(heroTag),
        "%.1f MS",
        static_cast<double>(s.frameMs)
    );
    std::snprintf(
        rateV, sizeof(rateV), "%.0f", static_cast<double>(hubFps(s))
    );
    std::snprintf(drawsV, sizeof(drawsV), "%u", s.stats.draws);
    int hitches = 0;
    for (const auto& h : hub->history())
      if (h.frameMs > 25.0f) ++hitches;
    std::snprintf(hitchV, sizeof(hitchV), "%d", hitches);
  } else {
    std::snprintf(frameV, sizeof(frameV), "--");
    std::snprintf(rateV, sizeof(rateV), "--");
    std::snprintf(drawsV, sizeof(drawsV), "--");
    std::snprintf(hitchV, sizeof(hitchV), "--");
    std::snprintf(heroTag, sizeof(heroTag), "IDLE");
  }
  return column(
      gap(20),
      // Status strip: pipeline health + the live driver switch.
      wide(column(
          gap(8),
          row(gap(8),
              lines("// PIPELINE"),
              column(expand(), gap(0)),
              lines("Live"),
              toggle({.value = live})),
          stageRow({.stages = hubStages(hub)})
      )),
      // KPI row: borderless stats on the window face.
      wide(row(
          gap(32),
          kpiCard({"// FRAME", frameV, "ms / budget 16.7"}, expand()),
          kpiCard(
              {"// RATE", rateV, has ? "fps / live" : "fps / idle"},
              expand()
          ),
          kpiCard({"// DRAWS", drawsV, "draws / frame"}, expand()),
          kpiCard({"// HITCHES", hitchV, "frames over 25ms"}, expand())
      )),
      // Hero: the live frame-time trace with a frosted title strip
      // floating over it — glass over real, varied content (bars),
      // so the blur reads. The stack paints the trace first
      // (backdrop), then the glass strip, then the strip's text
      // sharp on top.
      wide(card(
          radius(12),
          gap(8),
          pad(12),
          stack(
              stripChart({.ms = stripMs(hub), .height = 200.0f}),
              wide(
                  row(gap(8),
                      glass(10),
                      pad(8, 6),
                      lines("// FRAME TIME"),
                      column(expand(), gap(0)),
                      lines(heroTag))
              )
          )
      )),
      // Details: one panel, two columns. The card takes the max
      // child height, so both sections share a bottom edge.
      wide(card(
          radius(12),
          gap(8),
          pad(12),
          row(gap(24),
              column(
                  gap(8),
                  expand(),
                  lines("// STAGES"),
                  breakdownFromHub(hub)
              ),
              column(gap(8), expand(), alertFeed(log)))
      ))
  );
}

Block controlsPage(
    Scope app,
    State<int>& range,
    Anim<float>& rangePos,
    State<float>& level
) {
  auto taps = app.state("taps", 0);
  std::string count = "Pressed " + std::to_string(*taps) + " times";
  return column(
      gap(20),
      wide(card(
          radius(12),
          gap(8),
          pad(12),
          column(
              gap(8),
              lines("// BUTTONS"),
              row(gap(8),
                  button(
                      {.label = "Export",
                       .onTap =
                           [t = taps] { t.update([](int& v) { ++v; }); }
                      },
                      primary()
                  ),
                  button(
                      {.label = "Cancel", .onTap = [t = taps] { t = 0; }
                      }
                  )),
              lines(count)
          )
      )),
      wide(card(
          radius(12),
          gap(8),
          pad(12),
          column(
              gap(8),
              lines("// FIELD"),
              fieldRow({
                  .label = "Name",
                  .hint = "First and last name",
                  .id = kShowcaseSeatId,
              })
          )
      )),
      wide(card(
          radius(12),
          gap(8),
          pad(12),
          column(
              gap(8),
              lines("// TOGGLES"),
              row(gap(24),
                  toggle({.value = {"value", true}}),
                  checkbox({.label = "Sound"}))
          )
      )),
      wide(card(
          radius(12),
          gap(8),
          pad(12),
          column(
              gap(8),
              lines("// RANGE"),
              segmented(
                  {.options = {"Day", "Week", "Month"},
                   .selected = range,
                   .selPos = rangePos}
              )
          )
      )),
      wide(card(
          radius(12),
          gap(8),
          pad(12),
          column(
              gap(8),
              lines("// LEVEL"),
              slider({.value = level}),
              progress({.value = level})
          )
      ))
  );
}

Block systemPage(
    Scope,
    const host::TelemetryHub* hub,
    const std::vector<std::string>* log
) {
  Column logCol;
  logCol.gap = 4;
  if (log == nullptr || log->empty()) {
    logCol.children.push_back(lines("No events yet"));
  } else {
    const std::size_t from = log->size() > 12 ? log->size() - 12 : 0;
    for (std::size_t i = from; i < log->size(); ++i)
      logCol.children.push_back(lines((*log)[i]));
  }
  Block feed = toBlock(std::move(logCol));
  return column(
      gap(20),
      wide(card(
          radius(12),
          gap(8),
          pad(12),
          column(
              gap(8),
              lines("// FRAMES (PULL)"),
              stripChart({.ms = stripMs(hub), .height = 64.0f})
          )
      )),
      wide(card(radius(12), gap(8), pad(12), telemetrySummary(hub))),
      wide(card(radius(12), gap(8), pad(12), telemetryDetail(hub))),
      wide(card(
          radius(12),
          gap(8),
          pad(12),
          column(gap(4), lines("// PUSH LOG"), feed)
      )),
      wide(card(
          radius(12),
          gap(8),
          pad(12),
          column(
              gap(8),
              lines("// SURFACES"),
              lines("Solid elevations, one hairline, no frost")
          )
      ))
  );
}

}  // namespace

Block showcaseTree(
    Scope app,
    const host::TelemetryHub* hub,
    const std::vector<std::string>* log
) {
  auto page = app.state("page", 0);
  auto range = app.state("range", 1);
  Anim<float> rangePos{app, "range-pos", 1.0f};
  auto level = app.state("level", 0.4f);
  auto vscroll = app.state("vscroll", 0.0f);
  auto theme = app.state("theme", false);
  auto live = app.state("live", true);
  const int current = *page < 0 ? 0 : (*page > 2 ? 2 : *page);
  const char* names[3] = {"01 Overview", "02 Controls", "03 System"};
  const char* glyphs[3] = {"O", "C", "S"};
  Block content;
  if (current == 1) {
    content = controlsPage(app, range, rangePos, level);
  } else if (current == 2) {
    content = systemPage(app, hub, log);
  } else {
    content = overviewPage(app, hub, log, live);
  }
  auto navSel = [&](int index) {
    // Selection mirror: init matches on first build (headless paints
    // exact), retargets only on transitions.
    const bool want = current == index;
    Anim<float> a{
        app, "nav-sel-" + std::to_string(index), want ? 1.0f : 0.0f
    };
    auto prev = app.state("nav-prev-" + std::to_string(index), want);
    if (*prev != want) {
      a = want ? 1.0f : 0.0f;
      prev = want;
    }
    return a;
  };
  auto nav = [&](int index) {
    return navGlyph(
        {.label = names[index],
         .glyph = glyphs[index],
         .selected = current == index,
         .sel = navSel(index),
         .onTap = [p = page, index] { p = index; }}
    );
  };
  Block mark =
      column(gap(2), titleLines("Glim"), lines("Telemetry v1"));
  // Rail bottom: display toggle reusing the Switch element, then the
  // live link readout with a headline rate — the rail's FPS mirror
  // (the topbar pill is gone; one FPS home only).
  char linkV[32], linkD[32];
  if (hub != nullptr && !hub->empty()) {
    std::snprintf(
        linkV,
        sizeof(linkV),
        "%.0f",
        static_cast<double>(hubFps(hub->last()))
    );
    std::snprintf(linkD, sizeof(linkD), "fps / live");
  } else {
    std::snprintf(linkV, sizeof(linkV), "--");
    std::snprintf(linkD, sizeof(linkD), "fps / idle");
  }
  Block railTail = column(
      gap(8),
      lines("// DISPLAY"),
      row(gap(8),
          center(),
          lines("Light"),
          column(expand(), gap(0)),
          toggle({.value = theme})),
      column(gap(4), lines("// LINK"), valueLines(linkV), lines(linkD))
  );
  Block sidebar = sidePanel(
      {.width = kRailWidth},
      std::move(mark),
      column(gap(6), nav(0), nav(1), nav(2)),
      column(expand(), gap(0)),
      std::move(railTail)
  );
  // Header: floating flat-glass strip over the roll; content slides
  // under the frost. Square, no edge, dark-tinted plate.
  // Non-interactive: presses fall through, empty areas drag.
  Block header = wide(
      row(gap(12),
          center(),
          pad(16, 16),
          GlassTag{
              false, 0, GlassEdge::None, *theme ? 0u : kHeaderTintDark
          },
          titleLines(names[current]))
  );
  // Roll fills the column; the header floats over its top. The strut
  // clears the header at rest. No expand on the roll: the stack hands
  // it full constraints (see rollOf).
  Block main = stack(
      expand(),
      roll(
          vertical(),
          scrollOffset(vscroll),
          column(
              gap(0),
              pad(16),
              strut({.height = kHeaderH}),
              std::move(content)
          )
      ),
      std::move(header)
  );
  return showcaseThemed(
      *theme ? 1 : 0, row(gap(0), std::move(sidebar), std::move(main))
  );
}

}  // namespace toolkit
}  // namespace glim

namespace {

glim::toolkit::Engine engineFor(glim::toolkit::Store& store) {
  glim::toolkit::Engine e = glim::toolkit::host::measureEngine();
  e.store = store;
  return e;
}

}  // namespace

namespace glim {
namespace toolkit {

std::string telemetryLogLine(const host::FrameTelemetry& s) {
  char body[128];
  std::snprintf(
      body,
      sizeof(body),
      "t %.1fs frame %.1fms draws %u",
      static_cast<double>(s.time),
      static_cast<double>(s.frameMs),
      s.stats.draws
  );
  std::string head = "Frame";
  std::string sev;
  if (s.frameMs > 25.0f) {
    head = "HITCH";
    sev = "! ";
  } else if (s.frameMs > 20.0f || s.encodeMs > 8.0f) {
    head = "Slow";
    sev = "~ ";
  }
  return sev + head + " |" + body;
}

}  // namespace toolkit
}  // namespace glim

std::string dumpTree(Block tree, Constraints c) {
  std::ostringstream os;
  Engine e = host::measureEngine();
  Box box = e.place(tree, c);
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box, e);
  return os.str();
}

std::string showcaseDump(Store& store) {
  return dumpTree(
      showcaseTree(rootScope(engineFor(store)), nullptr, nullptr),
      {0, 1024, 0, 640}
  );
}

std::string showcaseFocusDump(Store& store) {
  Engine e = engineFor(store);
  Block tree = showcaseTree(rootScope(e), nullptr, nullptr);
  Constraints c{0, 1024, 0, 640};
  Box box = e.place(tree, c);
  std::ostringstream os;
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box, e);
  Box rb = focusRingBox(tree, box, 0, 2.0f, e);
  os << kFocusRingKind << " [" << rb.x << "," << rb.y << " " << rb.w
     << "x" << rb.h << "]\n";
  return os.str();
}

std::string showcaseA11yDump(Store& store) {
  Engine e = engineFor(store);
  Block tree = showcaseTree(rootScope(e), nullptr, nullptr);
  Box box = e.place(tree, {0, 1024, 0, 640});
  std::ostringstream os;
  dumpA11y(os, a11yNodes(tree, box, e));
  return os.str();
}

void paintShowcaseScene(
    glim::paint::Context& ctx,
    host::LayerFeed& feed,
    Store& store,
    int focus = -1,
    bool focusVisible = true
) {
  Engine engine = engineFor(store);
  Block screen = showcaseTree(rootScope(engine), nullptr, nullptr);
  Box box = engine.place(screen, {0, ctx.size().x, 0, ctx.size().y});
  host::paintScreen(
      ctx, screen, box, engine, feed, focus, focusVisible
  );
}

#ifndef GLIM_GOLDEN
#if defined(__ANDROID__)
extern "C" int glim_app_main() {
#else
int main() {
#endif
  // Dashboard owns telemetry both ways: the hub is pull (panels read it
  // in Build), the sink is push (a rolling classified log per frame:
  // "! " hitch, "~ " warn, else info — the feed parses the prefix).
  static host::TelemetryHub hub;
  static std::vector<std::string> pushLog;
  static Store::Watch liveWatch;
  host::TelemetrySink sink = [](const host::FrameTelemetry& s) {
    pushLog.push_back(telemetryLogLine(s));
    if (pushLog.size() > 40) pushLog.erase(pushLog.begin());
  };
  // One tree function; the host owns Store, Screen, Engine, and paint.
  // The live driver is user space: a never-settling frame watch keeps
  // the loop repainting (charts scroll, numbers move); the Live switch
  // drops/re-arms it and the loop idles. No core/host changes.
  return host::run(
      {.title = "OC | Modern dashboard showcase with telemetry",
       .width = 1240,
       .height = 760,
       .customTitlebar = true},
      [&](const Engine& e) {
        if (e.store) {
          Store& st = e.store->get();
          State<bool> live = st.state("live", true);
          if (*live && !liveWatch.live()) {
            State<float> phase = st.state("live-phase", 0.0f);
            liveWatch = st.watchFrame([phase](float now) mutable {
              phase = now;
              return true;
            });
          } else if (!*live) {
            liveWatch.reset();
          }
        }
        return showcaseTree(rootScope(e), &hub, &pushLog);
      },
      host::Record{},
      nullptr,
      nullptr,
      &hub,
      sink
  );
}
#endif
