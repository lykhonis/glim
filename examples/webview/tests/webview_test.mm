#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WebKit.h>

#include <glim/shell/Application.h>
#include <glim/shell/Window.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "Web.h"
#include "WebPage.h"
#include "glim/toolkit/host/Seats.h"
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

void spin(double seconds) { CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false); }

template <class Pred>
bool spinUntil(Pred done) {
  for (int i = 0; i < 200 && !done(); ++i) spin(0.05);
  return done();
}

id evaluate(WKWebView* web, NSString* script) {
  __block id result = nil;
  auto done = std::make_shared<bool>(false);
  [web evaluateJavaScript:script
        completionHandler:^(id value, NSError*) {
          result = value;
          *done = true;
        }];
  spinUntil([&] { return *done; });
  return result;
}

WKWebView* findWebView(NSView* root) {
  for (NSView* v in root.subviews) {
    if ([v isKindOfClass:[WKWebView class]]) return (WKWebView*)v;
  }
  return nil;
}

// Straight to the view: a test process may not become the active app, and
// AppKit routes clicks to inactive windows only to make them key.
void click(NSView* view, NSPoint at) {
  for (NSEventType type : {NSEventTypeLeftMouseDown, NSEventTypeLeftMouseUp}) {
    NSEvent* e = [NSEvent mouseEventWithType:type
                                    location:at
                               modifierFlags:0
                                   timestamp:NSProcessInfo.processInfo.systemUptime
                                windowNumber:view.window.windowNumber
                                     context:nil
                                 eventNumber:0
                                  clickCount:1
                                    pressure:1];
    if (type == NSEventTypeLeftMouseDown) {
      [view mouseDown:e];
    } else {
      [view mouseUp:e];
    }
    spin(0.05);
  }
}

// Why Apple is Slot: the only off-screen path is a snapshot per frame.
double snapshotMs(WKWebView* web, int count) {
  const CFTimeInterval start = CACurrentMediaTime();
  for (int i = 0; i < count; ++i) {
    auto done = std::make_shared<bool>(false);
    [web takeSnapshotWithConfiguration:nil
                     completionHandler:^(NSImage*, NSError*) {
                       *done = true;
                     }];
    while (!*done) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
  }
  return (CACurrentMediaTime() - start) * 1000.0 / count;
}

}  // namespace

// N7 gate: on Apple the page is a Slot. The seat lays out, the host seats a
// live WKWebView at its box, the page loads, and a real click reaches it.
int main() {
  @autoreleasepool {
    glim::shell::Application application;
    glim::shell::Window window;
    window.setSize(960, 640);
    window.show();
    spin(0.2);

    const std::unique_ptr<WebPage> page = makeWebPage();
    expect(page && page->seatViews() != nullptr, "Apple pages run in Slot mode");
    if (!page) return EXIT_FAILURE;

    host::TestApp app;
    app.w = 960;
    app.h = 640;
    app.build = [&](const Engine&) {
      return webTree({
        .mode = page->mode(),
        .size = {app.w, app.h},
        .slot = true,
        .seatId = kWebSeat,
      });
    };
    glim::paint::Context ctx;
    app.recordScene(ctx);
    std::vector<glim::paint::SlotPlacement> placed;
    glim::paint::collectSlots(ctx.scene(), &placed);
    expect(placed.size() == 1 && placed[0].id == kWebSeat, "the tree reserves one web seat");
    if (placed.size() != 1) return EXIT_FAILURE;
    const glim::Rect box = placed[0].windowLogical;
    expect(box.size.x == 960 - 48 && box.size.y > 400, "the seat fills the page area");

    host::EmbedFrames attached;
    host::syncSeats(window, ctx.scene(), attached, page->seatViews());
    NSView* root = (__bridge NSView*)window.nativeView();
    WKWebView* web = findWebView(root);
    expect(web != nil, "the host seated a WKWebView");
    if (!web) return EXIT_FAILURE;
    expect(
      std::fabs(web.frame.size.width - box.size.x) < 0.5 &&
        std::fabs(web.frame.size.height - box.size.y) < 0.5,
      "the web view takes the seat's size"
    );

    expect(
      spinUntil([&] { return !web.isLoading && [web.title isEqualToString:@"Glim WebView"]; }),
      "the page loads"
    );
    NSArray* rect = evaluate(
      web,
      @"(() => { const r = document.getElementById('tap').getBoundingClientRect();"
       " return [r.x + r.width / 2, r.y + r.height / 2]; })()"
    );
    expect(rect.count == 2, "the page reports its button");
    if (rect.count != 2) return EXIT_FAILURE;
    const NSPoint inPage = NSMakePoint([rect[0] doubleValue], [rect[1] doubleValue]);
    const NSPoint at = [web convertPoint:inPage toView:nil];
    expect([root.window.contentView hitTest:at] == web, "the web view is on top at the button");
    click(web, at);
    expect(
      spinUntil([&] { return [evaluate(web, @"taps") intValue] == 1; }),
      "a real click reaches the page"
    );

    std::cout << "webview snapshot: " << snapshotMs(web, 10) << " ms per frame at "
              << box.size.x << "x" << box.size.y << "\n";

    host::syncSeats(window, glim::paint::Scene{}, attached, page->seatViews());
    expect(web.superview == nil, "the seat leaving detaches the view");
  }
  if (failures == 0) std::cout << "webview slot: seated, loaded, clicked, detached\n";
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
