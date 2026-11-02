#import <TargetConditionals.h>

#include <glim/toolkit/host/SeatView.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

namespace glim {
namespace toolkit {
namespace host {

void* makeDefaultSeatView(SeatRequest request) {
  if (request.id == 0) return nullptr;
  const float w = request.bounds.size.x > 0 ? request.bounds.size.x : 100;
  const float h = request.bounds.size.y > 0 ? request.bounds.size.y : 40;
  // Factory-owned: SlotNative is embedder-owned (Glim never retains),
  // so the factory keeps a strong reference per id. addSubview retains
  // while attached; detach only removes from the hierarchy. Returning
  // a scope-local __bridge pointer would dangle at return — the old
  // code survived on unclobbered freed memory until heap churn caught
  // up with it.
  static NSMutableDictionary<NSNumber*, id>* live = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    live = [NSMutableDictionary new];
  });
  NSNumber* key = @(request.id);
  id existing = live[key];
  if (existing) return (__bridge void*)existing;
#if TARGET_OS_OSX
  NSTextField* field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
  field.placeholderString = @"Type here";
  live[key] = field;
  return (__bridge void*)field;
#else
  UITextField* field = [[UITextField alloc] initWithFrame:CGRectMake(0, 0, w, h)];
  field.placeholder = @"Type here";
  field.borderStyle = UITextBorderStyleRoundedRect;
  live[key] = field;
  return (__bridge void*)field;
#endif
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
