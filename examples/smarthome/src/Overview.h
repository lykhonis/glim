#pragma once

// Overview page: home sensors and controls on glass cards. Every
// number is a domain reading; switches, the thermostat ruler and the
// light dial bind device cells. Moving values arrive pre-eased in
// `Shown`, so the cards only draw them.

#include <cstdint>
#include <string>
#include <utility>

#include "Sensors.h"
#include "glim/toolkit.h"
#include "widgets/AreaChart.h"
#include "widgets/Card.h"
#include "widgets/Gauge.h"
#include "widgets/Grid.h"
#include "widgets/Meter.h"
#include "widgets/SignalBars.h"
#include "widgets/Switch.h"
#include "widgets/TickRuler.h"
#include "widgets/Type.h"
#include "widgets/Wide.h"

namespace glim {
namespace toolkit {

struct DeviceCells {
  State<bool> on;
  State<float> level;
};

// Displayed values: the goals below, mid-glide.
struct Shown {
  float energy = 0.0f;
  float indoorC = 0.0f;
  float thermostat = 0.0f;  // level
  float light = 0.0f;       // level
  float brightness = 0.0f;
  float solarShift = 0.0f;  // chart points
};

struct OverviewData {
  home::Readings readings;
  Shown shown;
  home::Device thermostat;
  DeviceCells thermostatCells;
  home::Device light;
  DeviceCells lightCells;
  State<bool> wifi;
  std::uint32_t glassTint = 0;
  bool day = true;
};

namespace overview {

inline Block spacer() { return column(expand(), gap(0)); }

inline Block stat(std::string label, std::string value) {
  return column(
      expand(),
      gap(6),
      lines(std::move(label)),
      valueLines(std::move(value))
  );
}

// Greeting over the villa sky, with the house at a glance beneath it.
inline Block hero(const OverviewData& d) {
  return column(
      gap(10),
      heroLines(home::greeting(d.day)),
      heroLines("Welcome home."),
      lines(home::homeSummary(d.readings, home::brightness({d.light})))
  );
}

inline Block energyCard(const OverviewData& d) {
  const home::Readings& r = d.readings;
  return glassCard(
      d.glassTint,
      radius(20),
      gap(14),
      semantic("energy"),
      titleLines("Energy"),
      row(gap(10),
          center(),
          valueLines(home::percentText(d.shown.energy)),
          lines(home::energyText(r.energy))),
      wide(meter({.value = d.shown.energy})),
      row(gap(0), lines("Today"), spacer(), lines("Daily budget"))
  );
}

inline Block climateCard(const OverviewData& d) {
  return glassCard(
      d.glassTint,
      radius(20),
      gap(12),
      semantic("climate"),
      row(gap(12),
          center(),
          titleLines("Living Room"),
          spacer(),
          toggle(
              {.value = d.thermostatCells.on}, semantic("Thermostat")
          )),
      wide(heroLines(
          home::temperatureText(d.shown.indoorC), alignCenter()
      )),
      wide(lines(
          home::climateText(d.readings.indoorC, d.thermostat),
          alignCenter(),
          a11y({.live = A11yLive::Polite})
      )),
      wide(tickRuler(
          {.value = d.thermostatCells.level,
           .shown = d.shown.thermostat,
           .lo = home::kThermoMinC,
           .hi = home::kThermoMaxC,
           .step = home::kThermoStepC},
          semantic("Target temperature"),
          a11y(
              {.value = home::temperatureText(
                   home::celsius(d.thermostat.level)
               )}
          )
      ))
  );
}

inline Block lightCard(const OverviewData& d) {
  return glassCard(
      d.glassTint,
      radius(20),
      gap(14),
      semantic("light"),
      row(gap(12),
          center(),
          titleLines("Ceiling Lights"),
          spacer(),
          toggle({.value = d.lightCells.on}, semantic("Ceiling lights"))
      ),
      row(gap(10),
          center(),
          valueLines(home::percentText(d.shown.brightness)),
          lines(home::lightText(d.light))),
      wide(gauge(
          {.value = d.lightCells.level,
           .shown = d.shown.light,
           .on = d.light.on},
          semantic("Brightness"),
          a11y({.value = home::percentText(d.light.level)})
      ))
  );
}

inline Block wifiCard(const OverviewData& d) {
  const home::Readings& r = d.readings;
  const bool on = *d.wifi;
  return glassCard(
      d.glassTint,
      radius(20),
      gap(14),
      semantic("wifi"),
      row(gap(10),
          center(),
          titleLines("Wi-Fi"),
          spacer(),
          toggle({.value = d.wifi}, semantic("Wi-Fi"))),
      row(gap(10),
          center(),
          signalBars({.bars = home::signalBars(r, on)}),
          lines("Glim_5G"),
          spacer(),
          lines(home::wifiStatus(r, on))),
      row(gap(16),
          stat("Latency", home::latencyText(r, on)),
          stat("Jitter", home::jitterText(r, on)))
  );
}

inline Block solarCard(const OverviewData& d) {
  const home::Readings& r = d.readings;
  return glassCard(
      d.glassTint,
      radius(20),
      gap(10),
      semantic("solar"),
      titleLines("Solar"),
      row(gap(10),
          center(),
          valueLines(home::kwhText(r.solarKwh)),
          lines("kWh generated today")),
      wide(areaChart(
          {.values = r.solarCurve,
           .slots = home::kLogBuckets,
           .shift = d.shown.solarShift,
           .labels = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"},
           .callout = home::kwhText(r.solarKwh) + " kWh"}
      ))
  );
}

}  // namespace overview

// The greeting holds the left over the photo and the controls cluster
// on the right: two panels, each a capped grid, that stack on narrow
// windows. Solar spans the bottom of the control panel.
inline constexpr int kGlassGap = 24;

inline Block overviewPage(const OverviewData& d) {
  Grid cards;
  cards.gap = kGlassGap;
  cards.minCell = 250.0f;
  cards.maxCols = 2;
  // Short readouts share the top row, the two tall controls the next.
  cards.children.push_back(overview::energyCard(d));
  cards.children.push_back(overview::wifiCard(d));
  cards.children.push_back(overview::climateCard(d));
  cards.children.push_back(overview::lightCard(d));
  Grid page;
  page.gap = kGlassGap;
  page.minCell = 520.0f;
  page.maxCols = 2;
  page.children.push_back(overview::hero(d));
  page.children.push_back(column(
      gap(kGlassGap),
      wide(grid(std::move(cards))),
      wide(overview::solarCard(d))
  ));
  return grid(std::move(page));
}

}  // namespace toolkit
}  // namespace glim
