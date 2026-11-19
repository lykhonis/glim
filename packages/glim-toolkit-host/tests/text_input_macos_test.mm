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

NSEvent* key(NSString* chars, unsigned short code, NSEventModifierFlags flags = 0) {
  return [NSEvent keyEventWithType:NSEventTypeKeyDown
                          location:NSZeroPoint
                     modifierFlags:flags
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

    // Standard shortcuts: the menu item a key equivalent names, sent down
    // the focused view's responder chain as AppKit does for the key window.
    const auto menuItem = [](NSString* k, NSEventModifierFlags mods) -> NSMenuItem* {
      for (NSMenuItem* top in NSApp.mainMenu.itemArray) {
        for (NSMenuItem* i in top.submenu.itemArray) {
          if ([i.keyEquivalent isEqualToString:k] && i.keyEquivalentModifierMask == mods) return i;
        }
      }
      return nil;
    };
    const auto shortcut = [&](NSString* k, unsigned short, NSEventModifierFlags extra = 0) {
      NSMenuItem* i = menuItem(k, NSEventModifierFlagCommand | extra);
      return i && [(id)view validateMenuItem:i] && [view tryToPerform:i.action with:i];
    };
    NSPasteboard* board = NSPasteboard.generalPasteboard;
    expect(shortcut(@"a", 0), "Cmd+A is a menu shortcut");
    expect(value->selection.start == 0 && value->selection.length == value->text.size(), "Cmd+A selects the field");
    expect(shortcut(@"c", 8), "Cmd+C is a menu shortcut");
    expect([[board stringForType:NSPasteboardTypeString] isEqualToString:@"hi!你"], "Cmd+C copies the selection");
    expect(shortcut(@"x", 7), "Cmd+X is a menu shortcut");
    expect(value->text.empty(), "Cmd+X cuts the selection");
    expect(shortcut(@"v", 9), "Cmd+V is a menu shortcut");
    expect(value->text == "hi!你", "Cmd+V pastes at the caret");
    // Text-system commands reach the model: word moves extend, forward
    // delete, and undo and redo through the Edit menu.
    [view doCommandBySelector:@selector(moveToBeginningOfDocument:)];
    [view doCommandBySelector:@selector(moveWordRightAndModifySelection:)];
    expect(value->selection.start == 0 && value->selection.length == 2, "word extend selects 'hi'");
    [view doCommandBySelector:@selector(deleteForward:)];
    expect(value->text == "!你", "forward delete removes the selection");
    expect(shortcut(@"z", 6), "Cmd+Z is a menu shortcut");
    expect(value->text == "hi!你", "Cmd+Z undoes");
    expect(shortcut(@"z", 6, NSEventModifierFlagShift), "Shift+Cmd+Z is a menu shortcut");
    expect(value->text == "!你", "Shift+Cmd+Z redoes");
    // A secure field neither copies nor cuts, with a live selection behind it.
    expect(shortcut(@"a", 0) && value->selection.length > 0, "select all before the secure check");
    [board clearContents];
    window.setTextInput({.active = true, .secure = true, .text = "••", .selection = {0, 6}});
    NSMenuItem* copyItem = menuItem(@"c", NSEventModifierFlagCommand);
    expect(![(id)view validateMenuItem:copyItem], "secure copy is disabled");
    [view tryToPerform:@selector(copy:) with:nil];
    [view tryToPerform:@selector(cut:) with:nil];
    expect([board stringForType:NSPasteboardTypeString] == nil, "secure fields never reach the clipboard");
    expect(value->text == "!你", "secure cut deletes nothing");
    publish();
    for (NSString* k : @[ @"h", @"m", @"w", @"q" ]) {
      expect(menuItem(k, NSEventModifierFlagCommand) != nil, std::string("menu has Cmd+") + k.UTF8String);
    }
    expect(menuItem(@"f", NSEventModifierFlagCommand | NSEventModifierFlagControl) != nil, "menu has Ctrl+Cmd+F");

    // Quit closes the windows, so the host's loop ends the way a closed
    // window ends it; the process is not torn down under the app.
    bool closed = false;
    window.setEventCallback([&](const glim::shell::Event& ev) {
      closed |= ev.type() == glim::shell::EventType::WindowClosed;
    });
    expect(
      [NSApp.mainMenu performKeyEquivalent:key(@"q", 12, NSEventModifierFlagCommand)],
      "Cmd+Q is a menu shortcut"
    );
    expect(closed, "Cmd+Q closes the window");
    expect(!view.window.visible, "the window is gone");
  }
  if (failures == 0) std::cout << "text input macos: typed, composed, deleted, edited by shortcut, quit\n";
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
