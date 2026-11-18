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
    if (WKWebView* inner = findWebView(v)) return inner;
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
    expect(box.size.x == 960 - 48 && box.size.y > 300, "the seat fills the page area");

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
    // A reflected page is layer-hosted even flat; Glim routes its clicks.
    click(root, at);
    expect(
      spinUntil([&] { return [evaluate(web, @"taps") intValue] == 1; }),
      "a real click reaches the page"
    );

    std::cout << "webview snapshot: " << snapshotMs(web, 10) << " ms per frame at "
              << box.size.x << "x" << box.size.y << "\n";

    // K88 Layer, K89 composition: under each motion the page stays a live
    // native view, composited by Core Animation under Glim's transform, and
    // its reflection is the same view again, flipped and faded. Pick the
    // motion and drag the amount slider to 30, the way a user would.
    NSView* layerHost = nil;
    const auto move = [&](const std::string& motion) {
      expect(press(motion), "the Glim motion button is there");
      app.rebuild();
      bool found = false;
      for (const A11yNode& n : a11yNodes(app.tree, app.box, app.engine())) {
        if (n.id != "Amount" || n.traits.role != A11yRole::Slider) continue;
        const float t = (30 + kWebAmountMax) / (2 * kWebAmountMax);
        const float x = n.box.x + Slider::kKnob / 2 + t * (n.box.w - Slider::kKnob);
        found = app.tap(x, n.box.y + n.box.h / 2);
      }
      expect(found && *screen().amount == 30, "the amount slider sets the app's amount");
      glim::paint::Context movedCtx;
      app.rebuild();
      app.recordScene(movedCtx);
      placed.clear();
      glim::paint::collectSlots(movedCtx.scene(), &placed);
      expect(placed.size() == 1 && placed[0].copies.size() == 1, "one seat, drawn again as its reflection");
      host::syncSeats(window, movedCtx.scene(), attached, page->seatViews());
      layerHost = web.superview;
      expect(layerHost != nil && layerHost != root, "the page sits in a layer host");
      return placed.empty() ? glim::paint::SlotPlacement{} : placed[0];
    };
    const auto projector = [](const glim::paint::SlotPlacement& s) {
      return [m = s.transform](glim::Vec2 local) {
        const glim::Vec3 q = glim::projectPlane(m, local);
        return glim::Vec2{q.x / q.z, q.y / q.z};
      };
    };
    // Core Animation draws each corner where Glim's paint puts it.
    const auto drawnWhereGlimPaints = [&](const glim::paint::SlotPlacement& s, const char* motion) {
      const auto projected = projector(s);
      const glim::Rect& drawn = s.windowLogical;
      expect(
        drawn.origin.y >= box.origin.y - 0.5f && drawn.origin.x >= box.origin.x - 0.5f &&
          drawn.origin.y + drawn.size.y <= box.origin.y + box.size.y + kWebReflect + 0.5f &&
          drawn.origin.x + drawn.size.x <= box.origin.x + box.size.x + 0.5f,
        "the moved page stays inside its area, clear of the chrome"
      );
      for (const glim::Vec2 c :
           {s.local.origin,
            glim::Vec2{s.local.origin.x + s.local.size.x, s.local.origin.y},
            glim::Vec2{s.local.origin.x, s.local.origin.y + s.local.size.y}}) {
        const CGPoint inHost = CGPointMake(c.x - s.local.origin.x, c.y - s.local.origin.y);
        const CGPoint ca = [root.layer convertPoint:inHost fromLayer:layerHost.layer];
        // In the flipped root view's points, which read y-down like Glim.
        const CGFloat caY = [root convertPointFromLayer:ca].y;
        const glim::Vec2 want = projected(c);
        if (std::fabs(ca.x - want.x) >= 1.0 || std::fabs(caY - want.y) >= 1.0) {
          std::cerr << motion << ": ";
        }
        expect(
          std::fabs(ca.x - want.x) < 1.0 && std::fabs(caY - want.y) < 1.0,
          "Core Animation draws the corner where Glim paints it"
        );
      }
    };
    // The replicator's second instance is the flip about the bottom edge,
    // and the mask holds the page whole and the copy fading from 0.35.
    const auto reflected = [&](const glim::paint::SlotPlacement& s) {
      auto* rep = (CAReplicatorLayer*)layerHost.layer;
      expect([rep isKindOfClass:CAReplicatorLayer.class] && rep.instanceCount == 2, "the page is replicated");
      const CGFloat h = s.local.size.y;
      const CATransform3D m = rep.instanceTransform;
      const auto mirrored = [&](CGFloat y) { return m.m22 * y + m.m42; };
      expect(
        std::fabs(mirrored(h) - h) < 0.5 && std::fabs(mirrored(0) - 2 * h) < 0.5,
        "the copy mirrors about the bottom edge"
      );
      NSArray<CALayer*>* drawings = rep.mask.sublayers;
      expect(drawings.count == 2, "the mask holds the page and its copy");
      if (drawings.count != 2) return;
      auto* copy = (CAGradientLayer*)drawings[1];
      expect(
        CATransform3DEqualToTransform(copy.transform, m) &&
          std::fabs(CGColorGetAlpha((CGColorRef)copy.colors[0]) - 0.35) < 1e-3 &&
          CGColorGetAlpha((CGColorRef)copy.colors[1]) == 0,
        "the copy's mask lies over it and fades from 0.35 to clear"
      );
      // What the window shows: the page's bright footer, mirrored just
      // under the seam, and the plain window once the fade has run out.
      // The readback needs the window on screen (WebKit stops drawing into
      // an occluded one) and Screen Recording permission (WebKit draws out
      // of process). CI runners have no permission, so there the structure
      // above is the gate.
      if (!(root.window.occlusionState & NSWindowOcclusionStateVisible)) {
        std::cerr << "SKIP readback: the window is not on screen\n";
        return;
      }
      if (!CGPreflightScreenCaptureAccess()) {
        std::cerr << "SKIP readback: no Screen Recording permission\n";
        return;
      }
      spin(0.4);
      CGImageRef shot = CGWindowListCreateImage(
        CGRectNull,
        kCGWindowListOptionIncludingWindow,
        static_cast<CGWindowID>(root.window.windowNumber),
        kCGWindowImageBoundsIgnoreFraming | kCGWindowImageNominalResolution
      );
      expect(shot != nullptr, "the window can be read back");
      if (!shot) return;
      NSBitmapImageRep* px = [[NSBitmapImageRep alloc] initWithCGImage:shot];
      CGImageRelease(shot);
      const auto lit = [&](glim::Vec2 local) {
        const glim::Vec2 at = projector(s)(local);
        const NSPoint w = [root convertPoint:NSMakePoint(at.x, at.y) toView:nil];
        NSColor* c = [[px colorAtX:static_cast<NSInteger>(w.x)
                                 y:px.pixelsHigh - 1 - static_cast<NSInteger>(w.y)]
          colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
        return c.redComponent + c.greenComponent + c.blueComponent;
      };
      const float cx = s.local.origin.x + s.local.size.x * 0.3f;
      const float bottom = s.local.origin.y + s.local.size.y;
      const double seam = lit({cx, bottom + 6});
      const double past = lit({cx, bottom + kWebReflect + 20});
      const double inside = lit({cx, bottom - 6});
      expect(seam > past + 0.25, "the reflection shows under the page and fades out");
      if (seam <= past + 0.25) {
        // CI uploads GLIM_GOT_DIR: the capture shows what the window drew.
        std::cerr << "readback page " << inside << " seam " << seam << " past " << past << " at "
                  << px.pixelsWide << "x" << px.pixelsHigh << "\n";
        if (const char* dir = std::getenv("GLIM_GOT_DIR")) {
          static int n = 0;
          NSString* path = [NSString stringWithFormat:@"%s/webview-readback-%d.png", dir, n++];
          [[px representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:path
                                                                                atomically:YES];
        }
      }
    };

    const glim::paint::SlotPlacement seat = move("Tilt");
    expect(seat.transform.is3D(), "the tilted seat carries a 3D transform");
    drawnWhereGlimPaints(seat, "Tilt");
    reflected(seat);
    const auto projected = projector(seat);
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
    const glim::Vec2 inReflection =
      projected({seat.local.origin.x + seat.local.size.x / 2, seat.local.origin.y + seat.local.size.y + kWebReflect / 3});
    click(root, [root convertPoint:NSMakePoint(inReflection.x, inReflection.y) toView:nil]);
    spin(0.3);
    expect([evaluate(web, @"taps") intValue] == 2, "a press on the reflection stays Glim's");

    // Skew and swing are the same primitive with other parts.
    for (const char* motion : {"Skew", "Swing", "Flat"}) {
      const glim::paint::SlotPlacement s = move(motion);
      drawnWhereGlimPaints(s, motion);
      reflected(s);
    }

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
  if (failures == 0) std::cout << "webview: seated, loaded, clicked, tilted, skewed and swung as a layer, reflected, routed, navigated by Glim chrome, detached\n";
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
