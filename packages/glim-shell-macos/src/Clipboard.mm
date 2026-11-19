#import <AppKit/AppKit.h>

#include <glim/shell/Clipboard.h>

namespace glim::shell {

ClipboardText readClipboard() {
  NSString* text = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
  if (!text) return {ClipboardStatus::Empty, {}};
  return {ClipboardStatus::Ok, text.UTF8String};
}

ClipboardStatus writeClipboard(std::string_view text) {
  NSString* s = [[NSString alloc] initWithBytes:text.data()
                                         length:text.size()
                                       encoding:NSUTF8StringEncoding];
  if (!s) return ClipboardStatus::Empty;
  NSPasteboard* board = NSPasteboard.generalPasteboard;
  [board clearContents];
  return [board setString:s forType:NSPasteboardTypeString] ? ClipboardStatus::Ok
                                                             : ClipboardStatus::Unsupported;
}

}  // namespace glim::shell
