#import <UIKit/UIKit.h>

#include <glim/shell/Window.h>

using glim::shell::PresentDone;
using glim::shell::PresentKind;
using glim::shell::PresentRequest;
using glim::shell::PresentResult;
using glim::shell::PresentStatus;

@interface GlimSharePresenter : NSObject
@property(nonatomic, weak) UIViewController* sheet;
@property(nonatomic, copy) void (^finish)(NSString*);
@end

@implementation GlimSharePresenter
@end

namespace glim::shell {

#if TARGET_OS_TV

void Window::present(const PresentRequest&, PresentDone done) {
  done({PresentStatus::Unsupported, {}});
}

void Window::dismissPresented() {}

#else

void Window::present(const PresentRequest& request, PresentDone done) {
  UIViewController* host = (__bridge UIViewController*)controller_;
  if (request.kind != PresentKind::Share || !host || !host.view.window) {
    done({PresentStatus::Unsupported, {}});
    return;
  }
  if (present_) {
    done({PresentStatus::Busy, {}});
    return;
  }
  NSString* text = [NSString stringWithUTF8String:request.text.c_str()];
  UIActivityViewController* sheet =
      [[UIActivityViewController alloc] initWithActivityItems:@[ text.length ? text : @"" ]
                                        applicationActivities:nil];
  GlimSharePresenter* presenter = [[GlimSharePresenter alloc] init];
  presenter.sheet = sheet;
  presenter.finish = ^(NSString* activity) {
    GlimSharePresenter* owned = (__bridge_transfer GlimSharePresenter*)present_;
    present_ = nullptr;
    owned.finish = nil;
    done(activity ? PresentResult{PresentStatus::Done, activity.UTF8String}
                  : PresentResult{PresentStatus::Cancelled, {}});
  };
  present_ = (__bridge_retained void*)presenter;
  sheet.completionWithItemsHandler = ^(UIActivityType type, BOOL completed, NSArray*, NSError*) {
    if (presenter.finish) {
      presenter.finish(completed ? (type ? type : @"") : nil);
    }
  };
  const Rect a = request.anchor;
  UIPopoverPresentationController* popover = sheet.popoverPresentationController;
  popover.sourceView = host.view;
  popover.sourceRect = a.size.x > 0 && a.size.y > 0
                         ? CGRectMake(a.origin.x, a.origin.y, a.size.x, a.size.y)
                         : CGRectMake(CGRectGetMidX(host.view.bounds),
                                      CGRectGetMidY(host.view.bounds), 1, 1);
  [host presentViewController:sheet animated:YES completion:nil];
}

void Window::dismissPresented() {
  if (!present_) {
    return;
  }
  GlimSharePresenter* presenter = (__bridge GlimSharePresenter*)present_;
  [presenter.sheet dismissViewControllerAnimated:NO completion:nil];
  if (presenter.finish) {
    presenter.finish(nil);
  }
}

#endif

}  // namespace glim::shell
