#pragma once

#include <glim/paint/Context.h>
#include <glim/toolkit.h>
#include <glim/toolkit/host/Paint.h>

#include <string>

// The app's headless surface: what tests and the GPU golden read.
std::string smartHomeDump(glim::toolkit::Store& store);
std::string smartHomeFocusDump(glim::toolkit::Store& store);
std::string smartHomeA11yDump(glim::toolkit::Store& store);
void paintSmartHomeScene(
  glim::paint::Context& ctx,
  glim::toolkit::host::LayerFeed& feed,
  glim::toolkit::Store& store,
  int focus = -1,
  bool focusVisible = true
);
