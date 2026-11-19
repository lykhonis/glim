#import <UIKit/UIKit.h>

#include <glim/shell/Clipboard.h>

namespace glim::shell {

// tvOS has no pasteboard.
#if TARGET_OS_TV
ClipboardText readClipboard() { return {}; }

ClipboardStatus writeClipboard(std::string_view) { return ClipboardStatus::Unsupported; }
#else
ClipboardText readClipboard() {
  NSString* text = UIPasteboard.generalPasteboard.string;
  if (!text) return {ClipboardStatus::Empty, {}};
  return {ClipboardStatus::Ok, text.UTF8String};
}

ClipboardStatus writeClipboard(std::string_view text) {
  NSString* s = [[NSString alloc] initWithBytes:text.data()
                                         length:text.size()
                                       encoding:NSUTF8StringEncoding];
  if (!s) return ClipboardStatus::Empty;
  UIPasteboard.generalPasteboard.string = s;
  return ClipboardStatus::Ok;
}
#endif

}  // namespace glim::shell
