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
  // Degrees about Y; the page turns in 3D, still live and still taking
  // input.
  float tilt = 0;
  WebStatus status;
  // The address bar's owned text field (N8).
  State<TextEdit> address;
  std::function<void(std::string)> go;
  std::function<void()> reload;
  std::function<void(float)> setTilt;
};

inline constexpr float kWebPad = 24;

// Labelled for screen readers by its text.
inline Block webButton(
  std::string label, Action action, bool on = false
) {
  const std::string name = label;
  return on ? tap(
                lines(std::move(label)),
                std::move(action),
                pad(12, 6),
                glass({.radius = 12}),
                primary(),
                semantic(name)
              )
            : tap(
                lines(std::move(label)),
                std::move(action),
                pad(12, 6),
                glass({.radius = 12}),
                semantic(name)
              );
}

// Glim's own chrome drives the native page through WebPage (K88
// bridge): address field, Local/Site, Go, Reload, load progress and
// tilt.
inline Block webChrome(const WebScreen& s) {
  const bool local = s.status.url == kLocalUrl;
  const auto go = s.go;
  const State<TextEdit> address = s.address;
  const float barW = s.size.x - 2 * kWebPad;
  // Site, Go and Reload with their gaps take about 200 px; the field
  // gets the rest.
  const float fieldW = std::max(80.0f, std::min(420.0f, barW - 212.0f));
  const float shown = s.status.loading ? s.status.progress : 1.0f;
  // One glass plate for the tilt presets; only the chosen one gets its
  // own. A glass container lenses at most kMaxLensPills plates, and the
  // chrome stays within it.
  const auto tiltTo = [set = s.setTilt, now = s.tilt](float t) {
    std::string label = std::to_string(static_cast<int>(t));
    const std::string name = "Tilt " + label;
    Action pick = [set, t] {
      if (set) set(t);
    };
    return now == t ? tap(
                        lines(std::move(label)),
                        std::move(pick),
                        pad(10, 4),
                        glass({.radius = 10}),
                        primary(),
                        semantic(name)
                      )
                    : tap(
                        lines(std::move(label)),
                        std::move(pick),
                        pad(10, 4),
                        semantic(name)
                      );
  };
  return column(
    gap(10),
    row(
      gap(12),
      row(pad(14, 8), glass({.radius = 14}), lines("Glim WebView")),
      lines(s.mode)
    ),
    row(
      gap(8),
      webButton(
        local ? "Site" : "Local",
        [go, local] {
          if (go) go(local ? kSiteUrl : kLocalUrl);
        }
      ),
      row(
        pad(6, 0),
        glass({.radius = 12}),
        textField(
          {.value = address, .width = fieldW, .placeholder = "Address"}
        )
      ),
      webButton(
        "Go",
        [go, address] {
          if (go) go(address->text);
        },
        true
      ),
      webButton(
        "Reload",
        [r = s.reload] {
          if (r) r();
        }
      )
    ),
    row(
      gap(8),
      lines("Tilt"),
      row(
        pad(2, 2),
        glass({.radius = 12}),
        tiltTo(0),
        tiltTo(15),
        tiltTo(30),
        tiltTo(45)
      )
    ),
    row(
      pad(static_cast<int>(barW * shown / 2), 2),
      glass({.radius = 2, .tint = 0x3a7bfdcc})
    ),
    lines(
      s.status.title.empty() ? s.status.url
                             : s.status.title + "  " + s.status.url
    )
  );
}

// The app's own state (address field, tilt) and the page wired to it.
inline WebScreen webScreen(
  WebPage* p, const Engine& e, glim::Vec2 size
) {
  Scope app = rootScope(e);
  State<TextEdit> address =
    app.state("address", TextEdit{.text = kLocalUrl});
  State<float> tilt = app.state("tilt", 0.0f);
  WebScreen s{
    .mode = p ? p->mode() : "No web view on this platform",
    .size = size,
    .tilt = *tilt,
    .address = address,
    .setTilt = [tilt](float t) { tilt = t; },
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

// A Slot or Layer view composites above the surface, so the chrome sits
// above the page. Texture pixels are Glim's, so the chrome floats on
// glass over the page there.
inline Block webTree(WebScreen s) {
  Block chrome = webChrome(s);
  if (s.slot) {
    return column(
      expand(),
      pad(kWebPad),
      gap(12),
      std::move(chrome),
      column(
        expand(),
        tilt({.y = s.tilt}),
        seat(
          {.id = s.seatId, .width = s.size.x - 2 * kWebPad},
          expand(),
          semantic("Web page")
        )
      )
    );
  }
  return stack(
    column(
      tilt({.y = s.tilt}),
      layer({
        .image = s.image,
        .onInput = std::move(s.input),
        .width = s.size.x,
        .height = s.size.y,
      })
    ),
    column(pad(kWebPad), std::move(chrome))
  );
}

}  // namespace toolkit
}  // namespace glim
