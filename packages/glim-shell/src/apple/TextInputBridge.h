#pragma once

#import <Foundation/Foundation.h>

#include <glim/shell/TextInput.h>

#include <string>

// The focused field as the host last published it, plus the callback its
// changes go to. UTF-16 is the platform's unit; the contract is UTF-8.
@interface GlimTextInput : NSObject
@property(nonatomic, readonly) BOOL active;
- (const glim::shell::TextInputState&)state;
// True when the caret moved, so the IME should re-read its anchor.
- (BOOL)publish:(glim::shell::TextInputState)state;
- (void)setCallback:(glim::shell::TextInputCallback)callback;
- (void)send:(glim::shell::TextInputChange)change;
- (NSRange)utf16:(glim::shell::TextInputRange)range;
// Edit commands shared by every Apple client. Secure fields never copy.
@property(nonatomic, readonly) BOOL canCopy;
@property(nonatomic, readonly) BOOL canPaste;
- (void)copySelection;
- (void)cutSelection;
- (void)paste;
@end

namespace glim::shell::detail {

std::string utf8(id string);
// UTF-16 units in [0, units) of a UTF-8 string → byte range.
TextInputRange bytes(const std::string& text, NSRange units);

}  // namespace glim::shell::detail
