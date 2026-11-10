#pragma once

// Home domain: rooms, devices and how their state reads. Plain values
// in, plain values out; the view binds State cells and only displays
// what these functions return.

#include <cstdio>
#include <string>
#include <vector>

namespace glim {
namespace home {

enum class Kind { Light, Climate, Speaker, Blinds };

struct Device {
  std::string id;
  std::string name;
  Kind kind = Kind::Light;
  bool on = false;
  float level = 0.0f;  // 0..1, meaning depends on kind
};

struct Room {
  std::string id;
  std::string name;
  std::vector<Device> devices;
};

// Seed home: the initial state of every device.
inline const std::vector<Room>& rooms() {
  static const std::vector<Room> kRooms = {
      {"living",
       "Living Room",
       {{"ceiling", "Ceiling Light", Kind::Light, true, 0.8f},
        {"lamp", "Floor Lamp", Kind::Light, false, 0.4f},
        {"climate", "Thermostat", Kind::Climate, true, 0.45f},
        {"speaker", "Speaker", Kind::Speaker, false, 0.3f},
        {"blinds", "Blinds", Kind::Blinds, true, 0.6f}}},
      {"kitchen",
       "Kitchen",
       {{"pendant", "Pendants", Kind::Light, true, 1.0f},
        {"counter", "Counter Strip", Kind::Light, false, 0.5f},
        {"radio", "Radio", Kind::Speaker, false, 0.2f}}},
      {"bedroom",
       "Bedroom",
       {{"bedside", "Bedside Lamp", Kind::Light, false, 0.25f},
        {"climate", "Thermostat", Kind::Climate, true, 0.3f},
        {"blinds", "Blinds", Kind::Blinds, false, 0.0f}}},
  };
  return kRooms;
}

// The thermostat scale: device level 0..1 spans these temperatures,
// set in whole steps.
inline constexpr float kThermoMinC = 16.0f;
inline constexpr float kThermoMaxC = 28.0f;
inline constexpr float kThermoStepC = 0.5f;

inline float celsius(float level) {
  return kThermoMinC + level * (kThermoMaxC - kThermoMinC);
}

inline int percent(float level) {
  return static_cast<int>(level * 100.0f + 0.5f);
}

inline std::string temperatureText(float c) {
  char out[16];
  std::snprintf(out, sizeof(out), "%.1f C", static_cast<double>(c));
  return out;
}

inline std::string percentText(float level) {
  return std::to_string(percent(level)) + "%";
}

// What the level means for this kind of device, e.g. "21.4 C".
inline std::string levelText(Kind kind, float level) {
  switch (kind) {
    case Kind::Climate:
      return temperatureText(celsius(level));
    case Kind::Speaker:
      return "Volume " + percentText(level);
    case Kind::Blinds:
      return "Open " + percentText(level);
    case Kind::Light:
      break;
  }
  return percentText(level);
}

inline std::string statusText(const Device& d) {
  return d.on ? levelText(d.kind, d.level) : std::string("Off");
}

inline int activeCount(const std::vector<Device>& devices) {
  int n = 0;
  for (const Device& d : devices)
    if (d.on) ++n;
  return n;
}

}  // namespace home
}  // namespace glim
