#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "WebPage.h"
#include "glim/math.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct WebScreen {
  const char* mode = "";
  glim::Vec2 size{};
  // Slot reserves the seat; Texture samples the image.
  bool slot = false;
  std::uint32_t seatId = 0;
  std::uint32_t image = 0;
  LayerInputFn input;
  // How the page moves, and how far: it stays live and keeps taking
  // input under every motion.
  State<int> motion;
  State<float> amount;
  WebStatus status;
  // The address bar's owned text field (N8).
  State<TextEdit> address;
  std::function<void(std::string)> go;
  std::function<void()> reload;
};

inline constexpr float kWebPad = 24;
inline constexpr float kWebAmountMax = 60;
// Room under the page for its live reflection.
inline constexpr float kWebReflect = 120;
// About the chrome's height, for the Texture plate's size.
inline constexpr float kWebChrome = 96;

enum class WebMotion : int { Flat, Tilt, Skew, Swing };
inline constexpr const char* kWebMotions[] = {
  "Flat", "Tilt", "Skew", "Swing"
};

// Each motion is one transform; amount runs
// -kWebAmountMax..kWebAmountMax. fit keeps the moved page inside its
// area, clear of the chrome.
inline TransformTag webMotion(WebMotion m, float amount) {
  switch (m) {
    case WebMotion::Flat:
      return {};
    case WebMotion::Tilt:
      return {.tilt = {0, amount}, .perspective = 900, .fit = true};
    case WebMotion::Skew:
      return {
        .skew = {amount / 3, 0}, .anchor = {0.5f, 1}, .fit = true
      };
    case WebMotion::Swing:
      return {
        .rotate = amount / 12,
        .tilt = {amount / 4, amount},
        .perspective = 700,
        .fit = true,
      };
  }
  return {};
}

// A plain text button, named for screen readers by its text; the
// selected one reads as primary.
inline Block webButton(
  std::string label, Action action, bool on = false
) {
  const std::string name = label;
  if (on)
    return tap(
      lines(std::move(label)),
      std::move(action),
      pad(8, 4),
      primary(),
      semantic(name)
    );
  return tap(
    lines(std::move(label)),
    std::move(action),
    pad(8, 4),
    semantic(name)
  );
}

inline std::string webStatusLine(const WebStatus& st) {
  std::string line =
    st.title.empty() ? st.url : st.title + "  " + st.url;
  if (st.loading)
    line += "  loading " +
            std::to_string(static_cast<int>(st.progress * 100)) + "%";
  return line;
}

inline Block webMotionPicker(const WebScreen& s) {
  const auto pick = [&s](WebMotion m) {
    const int i = static_cast<int>(m);
    return webButton(
      kWebMotions[i],
      [motion = s.motion, i] { motion = i; },
      *s.motion == i
    );
  };
  return row(
    gap(2),
    pick(WebMotion::Flat),
    pick(WebMotion::Tilt),
    pick(WebMotion::Skew),
    pick(WebMotion::Swing)
  );
}

// Glim's own chrome drives the native page through WebPage (K88
// bridge): one quiet glass bar with Local/Site, the address field, Go
// and Reload, then the motion picker and its amount, with the page
// status under it.
inline Block webChrome(const WebScreen& s) {
  const bool local = s.status.url == kLocalUrl;
  const auto go = s.go;
  const State<TextEdit> address = s.address;
  const float barW = s.size.x - 2 * kWebPad;
  // The buttons, the picker, the slider and the gaps take about 560 px.
  const float fieldW = std::max(80.0f, std::min(320.0f, barW - 560.0f));
  return column(
    gap(8),
    row(gap(12), lines("Glim WebView"), lines(s.mode)),
    row(
      gap(10),
      pad(12, 6),
      glass({.radius = 16, .merge = false}),
      webButton(
        local ? "Site" : "Local",
        [go, local] {
          if (go) go(local ? kSiteUrl : kLocalUrl);
        }
      ),
      textField(
        {.value = address, .width = fieldW, .placeholder = "Address"}
      ),
      webButton(
        "Go",
        [go, address] {
          if (go) go(address->text);
        }
      ),
      webButton(
        "Reload",
        [r = s.reload] {
          if (r) r();
        }
      ),
      webMotionPicker(s),
      slider(
        {.value = s.amount,
         .lo = -kWebAmountMax,
         .hi = kWebAmountMax,
         .step = 1,
         .width = 140},
        semantic("Amount")
      )
    ),
    lines(webStatusLine(s.status))
  );
}

// The app's own state (address field, motion) and the page wired to it.
inline WebScreen webScreen(
  WebPage* p, const Engine& e, glim::Vec2 size
) {
  Scope app = rootScope(e);
  State<TextEdit> address =
    app.state("address", TextEdit{.text = kLocalUrl});
  WebScreen s{
    .mode = p ? p->mode() : "No web view on this platform",
    .size = size,
    .motion = app.state("motion", static_cast<int>(WebMotion::Tilt)),
    .amount = app.state("amount", 0.0f),
    .address = address,
  };
  if (!p) return s;
  s.slot = p->seatViews() != nullptr;
  s.seatId = kWebSeat;
  s.image = p->acquire();
  s.input = [p](const LayerInput& in) { return p->input(in); };
  s.status = p->status();
  s.go = [p, address](std::string url) {
    address = TextEdit{.text = url, .selection = {url.size(), 0}};
    p->load(url);
  };
  s.reload = [p] { p->reload(); };
  return s;
}

// The live page: a seat for a Slot or Layer view, else the Texture
// plate. Both take the window less the chrome and the reflection's
// room.
inline Block webPage(const WebScreen& s) {
  const float w = s.size.x - 2 * kWebPad;
  const float h =
    std::max(1.0f, s.size.y - 2 * kWebPad - kWebChrome - kWebReflect);
  if (s.slot)
    return seat(
      {.id = s.seatId, .width = w, .height = h}, semantic("Web page")
    );
  return layer(
    {.image = s.image, .onInput = s.input, .width = w, .height = h}
  );
}

// A reflection is a composition (K89): the page drawn again, flipped
// about its bottom edge, fading out over kWebReflect, and inert.
inline Block webReflected(const WebScreen& s) {
  return stack(
    webPage(s),
    column(
      transform({.scale = {1, -1}, .anchor = {0.5f, 1}}),
      fade(
        {.edge = FadeEdge::Bottom,
         .length = kWebReflect,
         .fromAlpha = 0.35f,
         .toAlpha = 0}
      ),
      inert(),
      webPage(s)
    )
  );
}

// The chrome sits above the page; the page and its reflection move as
// one plane, with room below for the reflection.
inline Block webTree(WebScreen s) {
  return column(
    expand(),
    pad(kWebPad),
    gap(12),
    webChrome(s),
    column(
      expand(),
      transform(webMotion(static_cast<WebMotion>(*s.motion), *s.amount)
      ),
      webReflected(s),
      row(pad(0, static_cast<int>(kWebReflect / 2)))
    )
  );
}

}  // namespace toolkit
}  // namespace glim
