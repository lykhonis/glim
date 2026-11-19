// Runs the UIKit text input client on the simulator (xcrun simctl spawn):
// typing, composition, UIKit-set selections, replacement, UTF-16 ranges,
// hardware key commands, edit menu routing and secure entry.
#import "../src/TextView.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using glim::shell::TextInputChange;
using glim::shell::TextInputOp;
using glim::shell::TextInputState;
using glim::shell::TextInputUnit;

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  @autoreleasepool {
    GlimTextView* view = [[GlimTextView alloc] initWithFrame:CGRectZero];
    std::vector<TextInputChange> got;
    [view.textInput setCallback:[&](const TextInputChange& c) { got.push_back(c); }];

    // Inactive: no selection, and input goes nowhere.
    expect(view.selectedTextRange == nil && !view.canBecomeFirstResponder, "inactive client");
    [view insertText:@"x"];
    expect(got.empty(), "inactive input is dropped");

    // "a你b": 你 is 3 UTF-8 bytes and 1 UTF-16 unit.
    [view publish:{.active = true, .text = "a你b", .selection = {4, 0}, .caret = {{10, 20}, {2, 14}}}];
    expect(view.canBecomeFirstResponder && view.hasText, "active client");
    UITextRange* sel = view.selectedTextRange;
    expect([view offsetFromPosition:view.beginningOfDocument toPosition:sel.start] == 2, "caret in UTF-16");
    expect([view offsetFromPosition:view.beginningOfDocument toPosition:view.endOfDocument] == 3, "length in UTF-16");
    UITextRange* mid = [view textRangeFromPosition:[view positionFromPosition:view.beginningOfDocument offset:1]
                                        toPosition:[view positionFromPosition:view.beginningOfDocument offset:2]];
    expect([[view textInRange:mid] isEqualToString:@"你"], "text by UTF-16 range");
    expect([view positionFromPosition:view.endOfDocument offset:1] == nil, "no position past the end");
    expect(CGRectEqualToRect([view caretRectForPosition:sel.start], CGRectMake(10, 20, 2, 14)), "caret rect");

    // Typing, deleting and composing reach the field as changes.
    [view insertText:@"!"];
    [view deleteBackward];
    [view setMarkedText:@"ni" selectedRange:NSMakeRange(2, 0)];
    [view unmarkText];
    expect(got.size() == 4, "four changes");
    expect(got[0].op == TextInputOp::Insert && got[0].text == "!", "insert");
    expect(got[1].op == TextInputOp::DeleteBackward, "delete backward");
    expect(got[2].op == TextInputOp::Mark && got[2].text == "ni" && got[2].selection.start == 2, "mark");
    expect(got[3].op == TextInputOp::Unmark, "unmark");
    got.clear();
    [view insertText:@"\n"];
    expect(got.empty(), "return is not text in a single-line field");

    // UIKit sets the selection, and replaces ranges, in UTF-16; the field
    // receives bytes.
    view.selectedTextRange = mid;
    expect(got.size() == 1 && got[0].op == TextInputOp::Select, "select");
    expect(got[0].selection.start == 1 && got[0].selection.length == 3, "select in bytes");
    got.clear();
    [view replaceRange:mid withText:@"é"];
    expect(got.size() == 2 && got[0].op == TextInputOp::Select && got[1].op == TextInputOp::Insert, "replace");
    expect(got[0].selection.start == 1 && got[1].text == "é", "replace range and text");
    got.clear();

    // Marked text reports its UTF-16 range.
    [view publish:{.active = true, .text = "a你", .selection = {4, 0}, .marked = glim::shell::TextInputRange{1, 3}}];
    UITextRange* marked = view.markedTextRange;
    expect(marked && [view offsetFromPosition:view.beginningOfDocument toPosition:marked.start] == 1 &&
             [view offsetFromPosition:marked.start toPosition:marked.end] == 1,
           "marked range in UTF-16");

    // Hardware key commands go through the shared Cocoa command names.
    for (UIKeyCommand* c in view.keyCommands) {
      using Action = void (*)(id, SEL, id);
      reinterpret_cast<Action>([view methodForSelector:c.action])(view, c.action, c);
    }
    const std::vector<std::pair<TextInputOp, TextInputUnit>> want{
      {TextInputOp::Undo, TextInputUnit::Character},
      {TextInputOp::Redo, TextInputUnit::Character},
      {TextInputOp::DeleteForward, TextInputUnit::Character},
      {TextInputOp::DeleteForward, TextInputUnit::Word},
      {TextInputOp::DeleteBackward, TextInputUnit::Word},
      {TextInputOp::DeleteBackward, TextInputUnit::Line},
    };
    expect(got.size() == want.size(), "one change per key command");
    for (std::size_t i = 0; i < std::min(got.size(), want.size()); ++i) {
      expect(got[i].op == want[i].first && got[i].unit == want[i].second, "key command maps");
    }
    got.clear();

    // Edit menu. A spawned process has no pasteboard (UIPasteboard blocks),
    // so this checks enablement and routing; the copy, cut and paste logic
    // is the shared bridge's, which the macOS text input test drives
    // through a real pasteboard.
    [view publish:{.active = true, .text = "hello", .selection = {0, 5}}];
    expect([view canPerformAction:@selector(copy:) withSender:nil], "copy enabled");
    expect([view canPerformAction:@selector(selectAll:) withSender:nil], "select all enabled");
    [view selectAll:nil];
    expect(got.size() == 1 && got[0].op == TextInputOp::SelectAll, "select all");
    got.clear();

    // Secure: no copy or cut, no autocorrect, secure entry for the keyboard.
    [view publish:{.active = true, .secure = true, .text = "••", .selection = {0, 6}}];
    expect(![view canPerformAction:@selector(copy:) withSender:nil], "secure copy disabled");
    expect(![view canPerformAction:@selector(cut:) withSender:nil], "secure cut disabled");
    [view cut:nil];
    expect(got.empty(), "secure cut deletes nothing");
    expect(view.isSecureTextEntry && view.autocorrectionType == UITextAutocorrectionTypeNo, "secure keyboard");

    // Losing focus drops the client.
    [view publish:{}];
    expect(!view.canBecomeFirstResponder && view.selectedTextRange == nil, "inactive again");

#if TARGET_OS_TV
    // tvOS: focus alone opens no keyboard; Select opens the system keyboard
    // on a hidden UITextField, and its edits replace the field's text.
    UIView* host = [UIView new];
    [host addSubview:view];
    [view publish:{.active = true, .text = "ab", .selection = {2, 0}}];
    expect(!view.isFirstResponder, "focus alone opens no keyboard");
    [view openKeyboard];
    UITextField* kb = view.keyboardField;
    expect(kb.superview == host && [kb.text isEqualToString:@"ab"], "the keyboard field holds the text");
    kb.text = @"abc";
    [kb sendActionsForControlEvents:UIControlEventEditingChanged];
    expect(got.size() == 2 && got[0].op == TextInputOp::Select && got[0].selection.start == 0 &&
             got[0].selection.length == 2 && got[1].op == TextInputOp::Insert && got[1].text == "abc",
           "keyboard edits replace the text");
    got.clear();
    // A secure field starts the keyboard empty: its published text is bullets.
    [view publish:{.active = true, .secure = true, .text = "••", .selection = {6, 0}}];
    [view openKeyboard];
    expect(kb.secureTextEntry && kb.text.length == 0, "secure keyboard starts empty");
    [view publish:{}];
    expect(![view openKeyboard], "no keyboard without a field");
#endif
  }
  if (failures == 0) std::cout << "ios text view: typed, composed, selected, replaced, commanded, edited, secured\n";
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
