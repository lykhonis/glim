#pragma once

#include <vector>

#include "Home.h"
#include "Overview.h"
#include "Sensors.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

namespace host {
struct FrameTelemetry;
}

// Glim Home: one page of glass cards under a glass header. The
// sensors read `log`; the demo log stands in when there is no host.
Block smartHomeTree(
    Scope app, const home::SensorLog& log = home::demoLog()
);

// One painted frame as a sensor sample.
home::FrameSample frameSample(const host::FrameTelemetry& t);

// State cells for one device, seeded from the home's initial values.
DeviceCells deviceCells(
    Scope app, const home::Room& room, const home::Device& seed
);

// Live devices of a room: the seed with State cells applied.
std::vector<home::Device> liveDevices(
    Scope app, const home::Room& room
);

}  // namespace toolkit
}  // namespace glim
