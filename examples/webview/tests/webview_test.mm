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
    app.build = [&](const Engine& e) { return webTree(webScreen(page.get(), e, {app.w, app.h})); };
    // The app's own wiring: its state, its chrome, its page.
    const auto screen = [&] { return webScreen(page.get(), app.engine(), {app.w, app.h}); };
    // Taps a chrome button by its accessible name, the way a user would.
    const auto press = [&](const std::string& label) {
      app.rebuild();
      for (const A11yNode& n : a11yNodes(app.tree, app.box, app.engine())) {
        if (n.id == label && n.traits.role == A11yRole::Button) {
          return app.tap(n.box.x + n.box.w / 2, n.box.y + n.box.h / 2);
        }
      }
      return false;
    };
    const auto settle = [&](const auto& done) {
      return spinUntil([&] {
        page->advance();
        return done(page->status());
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

    // K88 Layer: tilted, the page stays a live native view, composited by
    // Core Animation under Glim's transform, and Glim routes its clicks.
    expect(press("Tilt 30"), "the Glim tilt button is there");
    expect(screen().tilt == 30, "the tilt button sets the app's tilt");
    glim::paint::Context tiltedCtx;
    app.rebuild();
    app.recordScene(tiltedCtx);
    placed.clear();
    glim::paint::collectSlots(tiltedCtx.scene(), &placed);
    expect(placed.size() == 1 && placed[0].transform.is3D(), "the tilted seat carries a 3D transform");
    if (placed.size() != 1) return EXIT_FAILURE;
    const glim::paint::SlotPlacement seat = placed[0];
    host::syncSeats(window, tiltedCtx.scene(), attached, page->seatViews());
    NSView* layerHost = web.superview;
    expect(layerHost != nil && layerHost != root, "the tilted page moves into a layer host");
    const auto projected = [&](glim::Vec2 local) {
      const glim::Vec3 q = glim::projectPlane(seat.transform, local);
      return glim::Vec2{q.x / q.z, q.y / q.z};
    };
    for (const glim::Vec2 c :
         {seat.local.origin,
          glim::Vec2{seat.local.origin.x + seat.local.size.x, seat.local.origin.y},
          glim::Vec2{seat.local.origin.x, seat.local.origin.y + seat.local.size.y}}) {
      const CGPoint inHost = CGPointMake(c.x - seat.local.origin.x, c.y - seat.local.origin.y);
      const CGPoint ca = [root.layer convertPoint:inHost fromLayer:layerHost.layer];
      // The root layer reads y-up; the flipped root view and Glim read y-down.
      const CGFloat caY = root.layer.geometryFlipped ? root.layer.bounds.size.height - ca.y : ca.y;
      const glim::Vec2 want = projected(c);
      expect(
        std::fabs(ca.x - want.x) < 1.0 && std::fabs(caY - want.y) < 1.0,
        "Core Animation draws the corner where Glim projects it"
      );
    }
    const glim::Vec2 button = projected(
      {seat.local.origin.x + static_cast<float>(inPage.x),
       seat.local.origin.y + static_cast<float>(inPage.y)}
    );
    const NSPoint buttonInWindow = [root convertPoint:NSMakePoint(button.x, button.y) toView:nil];
    click(root, buttonInWindow);
    expect(
      spinUntil([&] { return [evaluate(web, @"taps") intValue] == 2; }),
      "a click on the projected button reaches the page"
    );
    // The far top corner of the flat rect lies outside the projected quad:
    // that press is Glim's, not the page's.
    const NSPoint flatCorner = [root
      convertPoint:NSMakePoint(
                     seat.local.origin.x + seat.local.size.x - 3, seat.local.origin.y + 3
                   )
            toView:nil];
    click(root, flatCorner);
    spin(0.3);
    expect([evaluate(web, @"taps") intValue] == 2, "a press outside the projected page stays Glim's");

    screen().setTilt(0);
    glim::paint::Context flatCtx;
    app.rebuild();
    app.recordScene(flatCtx);
    host::syncSeats(window, flatCtx.scene(), attached, page->seatViews());
    expect(web.superview == root, "flat again, the page is a plain sibling");

    // Bridged chrome (K88): Glim's own address field and buttons drive the page.
    screen().address = TextEdit{.text = "about:blank"};
    expect(press("Go"), "the Glim Go button is there");
    expect(
      settle([](const WebStatus& s) { return !s.loading && s.url == "about:blank"; }),
      "Go navigates the page to the typed address"
    );
    expect(press("Local"), "the Glim Local button is there");
    expect(
      settle([](const WebStatus& s) {
        return !s.loading && s.url == kLocalUrl && s.title == "Glim WebView";
      }),
      "Local brings the bundled page back"
    );
    expect(screen().address->text == kLocalUrl, "the address field follows");

    host::syncSeats(window, glim::paint::Scene{}, attached, page->seatViews());
    expect(web.superview == nil, "the seat leaving detaches the view");
  }
  if (failures == 0) std::cout << "webview: seated, loaded, clicked, tilted as a layer, routed, navigated by Glim chrome, detached\n";
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
