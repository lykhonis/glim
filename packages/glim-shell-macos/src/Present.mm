#import <AppKit/AppKit.h>

#include <glim/shell/Window.h>

using glim::shell::PresentDone;
using glim::shell::PresentKind;
using glim::shell::PresentRequest;
using glim::shell::PresentResult;
using glim::shell::PresentStatus;

@interface GlimSharePresenter : NSObject <NSSharingServicePickerDelegate>
@property(nonatomic, strong) NSSharingServicePicker* picker;
@property(nonatomic, copy) void (^finish)(NSSharingService*);
@end

@implementation GlimSharePresenter
- (void)sharingServicePicker:(NSSharingServicePicker*)picker
     didChooseSharingService:(NSSharingService*)service {
  if (self.finish) {
    self.finish(service);
  }
}
@end

namespace glim::shell {

void Window::present(const PresentRequest& request, PresentDone done) {
  NSView* view = (__bridge NSView*)view_;
  if (request.kind != PresentKind::Share || !view) {
    done({PresentStatus::Unsupported, {}});
    return;
  }
  if (present_) {
    done({PresentStatus::Busy, {}});
    return;
  }
  NSString* text = [NSString stringWithUTF8String:request.text.c_str()];
  GlimSharePresenter* presenter = [[GlimSharePresenter alloc] init];
  presenter.picker = [[NSSharingServicePicker alloc] initWithItems:@[ text.length ? text : @"" ]];
  presenter.picker.delegate = presenter;
  presenter.finish = ^(NSSharingService* service) {
    GlimSharePresenter* owned = (__bridge_transfer GlimSharePresenter*)present_;
    present_ = nullptr;
    owned.finish = nil;
    done(service ? PresentResult{PresentStatus::Done, service.title.UTF8String}
                 : PresentResult{PresentStatus::Cancelled, {}});
  };
  present_ = (__bridge_retained void*)presenter;
  const Rect a = request.anchor;
  const NSRect rect = a.size.x > 0 && a.size.y > 0
                        ? NSMakeRect(a.origin.x, a.origin.y, a.size.x, a.size.y)
                        : NSMakeRect(NSMidX(view.bounds), NSMidY(view.bounds), 1, 1);
  [presenter.picker showRelativeToRect:rect ofView:view preferredEdge:NSRectEdgeMinY];
}

void Window::dismissPresented() {
  if (!present_) {
    return;
  }
  GlimSharePresenter* presenter = (__bridge GlimSharePresenter*)present_;
  [presenter.picker close];
  if (presenter.finish) {
    presenter.finish(nil);
  }
}

}  // namespace glim::shell
