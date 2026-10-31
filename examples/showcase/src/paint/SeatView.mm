#import <TargetConditionals.h>

#include <cstdint>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

// First native field, macOS + iOS smoke: a plain text field hosted in
// the SlotHole the toolkit Seat reserves. ARC owns the field while it
// is in the view hierarchy (addSubview retains); detach removes it.
// No styling — the hole geometry is the contract under test. Other
// shells use the track-only stub (SeatView.cpp).
namespace glim {
namespace toolkit {
namespace paint {

void* glimMakeSeatView(std::uint32_t id) {
  (void)id;
#if TARGET_OS_OSX
  NSTextField* field =
      [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 40)];
  field.placeholderString = @"Type here";
  return (__bridge void*)field;
#else
  UITextField* field =
      [[UITextField alloc] initWithFrame:CGRectMake(0, 0, 100, 40)];
  field.placeholder = @"Type here";
  field.borderStyle = UITextBorderStyleRoundedRect;
  return (__bridge void*)field;
#endif
}

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
