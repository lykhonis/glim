#import <AppKit/AppKit.h>

#include <glim/shell/Application.h>
#include <glim/shell/Window.h>
#include <glim/toolkit.h>
#include <glim/toolkit/host/TextInput.h>

#include <cstdlib>
#include <iostream>
#include <string>

using namespace glim::toolkit;

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

NSEvent* key(NSString* chars, unsigned short code) {
  return [NSEvent keyEventWithType:NSEventTypeKeyDown
                          location:NSZeroPoint
                     modifierFlags:0
                         timestamp:0
                      windowNumber:0
                           context:nil
                        characters:chars
       charactersIgnoringModifiers:chars
                         isARepeat:NO
                           keyCode:code];
}

}  // namespace

// N8 gate (macOS): the view is the NSTextInputClient for the focused owned
// field. Typing, composition, UTF-16 ranges, the candidate anchor and
// deletion all round-trip through host::run's mapping into the field.
int main() {
  @autoreleasepool {
    glim::shell::Application application;
    glim::shell::Window window;
    window.setSize(480, 320);
    window.show();

    Store store;
    Engine e;
    e.store = store;
    State<TextEdit> value = rootScope(e).state("name", TextEdit{});
    Screen screen;
    const auto build = [&] {
      return column(pad(20), textField({.value = value, .width = 240}));
    };
    const auto publish = [&] {
      Block tree = build();
      const Box box = e.place(tree, {0, 480, 0, 320});
      window.setTextInput(host::shellTextInput(screen.textFocus(tree, box, e)));
    };
    window.setTextInputCallback([&](const glim::shell::TextInputChange& change) {
      Block tree = build();
      const Box box = e.place(tree, {0, 480, 0, 320});
      screen.text(tree, box, host::toolkitText(change), e);
      publish();
    });
    NSView<NSTextInputClient>* view = (__bridge NSView<NSTextInputClient>*)window.nativeView();

    // Unfocused: keys and the IME find no client state.
    publish();
    expect([view selectedRange].location == NSNotFound, "no field, no selection");
    [view insertText:@"x" replacementRange:NSMakeRange(NSNotFound, 0)];
    expect(value->text.empty(), "unfocused text goes nowhere");

    {
      Block tree = build();
      const Box box = e.place(tree, {0, 480, 0, 320});
      const Box field = focusBox(tree, box, 0, e);
      screen.pointerDown(tree, box, field.x + 4, field.y + 4, 1, e);
    }
    publish();
    expect(view.inputContext != nil, "the view has an input context");

    // Plain typing through key bindings.
    [view keyDown:key(@"h", 4)];
    [view keyDown:key(@"i", 34)];
    expect(value->text == "hi", "typed keys insert");

    // Composition: marked text, UTF-16 ranges, then commit.
    [view setMarkedText:@"ni" selectedRange:NSMakeRange(2, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
    expect(value->text == "hini" && value->marked, "marked text opens a composition");
    expect([view hasMarkedText], "the client reports marked text");
    expect(NSEqualRanges([view markedRange], NSMakeRange(2, 2)), "marked range in UTF-16");
    const NSRect anchor = [view firstRectForCharacterRange:[view markedRange] actualRange:nullptr];
    const NSRect frame = window.nativeView() ? ((__bridge NSView*)window.nativeView()).window.frame : NSZeroRect;
    expect(NSPointInRect(anchor.origin, NSInsetRect(frame, -1, -1)), "candidate anchor is on screen in the window");
    [view insertText:@"你好" replacementRange:NSMakeRange(NSNotFound, 0)];
    expect(value->text == "hi你好" && !value->marked, "commit replaces the marked run");
    expect(NSEqualRanges([view selectedRange], NSMakeRange(4, 0)), "selection counts UTF-16 units");
    NSString* sub = [[view attributedSubstringForProposedRange:NSMakeRange(2, 2) actualRange:nullptr] string];
    expect([sub isEqualToString:@"你好"], "substring by UTF-16 range");

    // Deletion and caret moves through key bindings.
    [view keyDown:key(@"\x7f", 51)];
    expect(value->text == "hi你", "backspace deletes a whole code point");
    [view keyDown:key([NSString stringWithFormat:@"%C", (unichar)NSLeftArrowFunctionKey], 123)];
    [view keyDown:key(@"!", 18)];
    expect(value->text == "hi!你", "left arrow then typing inserts before");

    // Cancelling a composition with an empty mark leaves the text.
    [view setMarkedText:@"k" selectedRange:NSMakeRange(1, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
    [view setMarkedText:@"" selectedRange:NSMakeRange(0, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
    expect(value->text == "hi!你" && ![view hasMarkedText], "empty mark cancels");

    // Focus leaving deactivates the client.
    screen.focus = -1;
    publish();
    expect([view selectedRange].location == NSNotFound, "focus loss deactivates");
  }
  if (failures == 0) std::cout << "text input macos: typed, composed, deleted, deactivated\n";
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
