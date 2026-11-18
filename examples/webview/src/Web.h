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
  State<float> tilt;
  WebStatus status;
  // The address bar's owned text field (N8).
  State<TextEdit> address;
  std::function<void(std::string)> go;
  std::function<void()> reload;
};

inline constexpr float kWebPad = 24;
inline constexpr float kWebTiltMax = 60;

// Chrome plates sit close together; merged, they would melt into one
// droplet, so each keeps its own outline.
inline GlassTag webGlass(int radius) {
  return glass({.radius = radius, .merge = false});
}

// Labelled for screen readers by its text.
inline Block webButton(
  std::string label, Action action, bool on = false
) {
  const std::string name = label;
  return on ? tap(
                lines(std::move(label)),
                std::move(action),
                pad(12, 6),
                webGlass(12),
                primary(),
                semantic(name)
              )
            : tap(
                lines(std::move(label)),
                std::move(action),
                pad(12, 6),
                webGlass(12),
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
  return column(
    gap(10),
    row(
      gap(12),
      row(pad(14, 8), webGlass(14), lines("Glim WebView")),
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
        webGlass(12),
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
      gap(12),
      lines("Tilt"),
      slider(
        {.value = s.tilt,
         .lo = -kWebTiltMax,
         .hi = kWebTiltMax,
         .step = 1,
         .width = 260},
        semantic("Tilt")
      ),
      lines(std::to_string(static_cast<int>(*s.tilt)))
    ),
    row(
      pad(static_cast<int>(barW * shown / 2), 2),
      glass({.radius = 2, .tint = 0x3a7bfdcc, .merge = false})
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
    .tilt = tilt,
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
        tilt({.y = *s.tilt, .perspective = 900, .fit = true}),
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
      tilt({.y = *s.tilt, .perspective = 900, .fit = true}),
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
