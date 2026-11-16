#import <AppKit/AppKit.h>

#include <glim/shell/Application.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Window.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "Video.h"
#include "glim/toolkit/host/A11y.h"
#include "glim/toolkit/host/Host.h"
#include "glim/toolkit/host/TestApp.h"

using namespace glim::toolkit;

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

id find(NSArray* elements, NSString* label) {
  for (id e in elements) {
    if ([[e accessibilityLabel] isEqualToString:label]) return e;
    if (id hit = find([e accessibilityChildren], label)) return hit;
  }
  return nil;
}

NSRect inView(NSView* view, id element) {
  const NSRect screen = [element accessibilityFrame];
  return [view convertRect:[view.window convertRectFromScreen:screen] fromView:nil];
}

bool near(NSRect a, const Box& b) {
  return std::fabs(a.origin.x - b.x) < 0.5 && std::fabs(a.origin.y - b.y) < 0.5 &&
         std::fabs(a.size.width - b.w) < 0.5 && std::fabs(a.size.height - b.h) < 0.5;
}

}  // namespace

// N5 gate: the video's own subtree, exported in clip space, reaches the
// macOS accessibility tree under the plate with frames on the plate.
int main() {
  @autoreleasepool {
    glim::shell::Application application;
    glim::shell::Window window;
    window.setSize(960, 640);
    window.show();
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.2, false);
    NSView* view = (__bridge NSView*)window.nativeView();

    host::TestApp app;
    app.w = 960;
    app.h = 640;
    app.build = [](const Engine& e) {
      return videoTree({.image = 1, .playing = rootScope(e).state("playing", true)});
    };
    app.rebuild();
    const std::vector<A11yNode> nodes = a11yNodes(app.tree, app.box, app.engine());

    expect(!window.a11yActive(), "no tree before the OS asks");
    NSArray* roots = [view accessibilityChildren];
    expect(window.a11yActive(), "asking turns the bridge on");
    window.setA11y(host::shellA11y(nodes));
    roots = [view accessibilityChildren];

    id video = find(roots, @"Video");
    id clip = find(roots, @"Sunset over a lake");
    id sun = find(roots, @"Sun");
    expect(video && [[video accessibilityRole] isEqualToString:NSAccessibilityButtonRole],
           "the video is a button");
    expect(clip && sun && find(@[ clip ], @"Hills") && find(@[ clip ], @"Lake"),
           "the clip's subtree is in the tree");
    expect(sun && [sun accessibilityParent] == clip, "the sun sits under the clip");

    const Box plate = [&] {
      for (const A11yNode& n : nodes)
        if (n.kind == "VideoPlate") return n.box;
      return Box{};
    }();
    const float s = plate.w / 640.0f;
    expect(plate.w > 0 && clip && near(inView(view, clip), plate), "the clip covers the plate");
    expect(sun && near(inView(view, sun),
                       {plate.x + 420 * s, plate.y + 125 * s, 30 * s, 30 * s}),
           "the sun maps from clip space onto the plate");
    const NSRect sunScreen = sun ? [sun accessibilityFrame] : NSZeroRect;
    expect([view accessibilityHitTest:NSMakePoint(NSMidX(sunScreen), NSMidY(sunScreen))] == sun,
           "pointing at the sun finds it");

    int activated = -1;
    window.setA11yPerform([&](int index, glim::shell::A11yCommand) { activated = index; });
    expect([video accessibilityPerformPress], "the video accepts press");
    expect(activated >= 0 && nodes[static_cast<std::size_t>(activated)].id == "Video",
           "press names the video node");
    window.setA11y(host::shellA11y(nodes));
    expect(find([view accessibilityChildren], @"Sun") == sun, "an unchanged tree keeps its elements");

    std::vector<glim::shell::A11yCommand> commands;
    window.setA11yPerform([&](int, glim::shell::A11yCommand c) { commands.push_back(c); });
    expect(![video accessibilityPerformIncrement], "a plain button refuses increment");
    window.setA11y({{.role = glim::shell::A11yRole::Slider, .label = "Volume", .adjustable = true}});
    id volume = find([view accessibilityChildren], @"Volume");
    expect([volume accessibilityPerformIncrement] && [volume accessibilityPerformDecrement],
           "an adjustable slider takes increment and decrement");
    expect(commands == std::vector{glim::shell::A11yCommand::Increment,
                                   glim::shell::A11yCommand::Decrement},
           "both reach the host in order");
  }
  // The live host: once the OS asks, run() publishes the tree, and a
  // screen-reader press taps the node it names.
  @autoreleasepool {
    bool published = false;
    bool pressed = false;
    bool paused = false;
    int polls = 0;
    glim::shell::RunLoop().scheduleRepeatedTask([&](glim::shell::RunLoop::TaskContext& task) {
      NSView* view = nil;
      for (NSWindow* w in NSApp.windows) {
        if ([w.title isEqualToString:@"Glim Video a11y"]) view = w.contentView.subviews.firstObject;
      }
      NSArray* roots = [view accessibilityChildren];
      if (!published && find(roots, @"Sun")) {
        published = true;
        pressed = [find(roots, @"Video") accessibilityPerformPress];
      }
      paused = [[find(roots, @"Video") accessibilityValue] isEqual:@"Paused"];
      if (paused || ++polls > 2000) {
        task.cancel();
        glim::shell::RunLoop().stop();
      }
    });
    host::run(
      {.title = "Glim Video a11y", .width = 960, .height = 640},
      [](const Engine& e) {
        return videoTree({.image = 1, .playing = rootScope(e).state("playing", true)});
      }
    );
    expect(published, "run() publishes once the OS asks");
    expect(pressed, "the live video accepts press");
    expect(paused, "the press paused the video and the tree says so");
  }
  if (failures == 0) std::cout << "a11y: clip subtree grafted under the plate, frames mapped; live press paused\n";
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
