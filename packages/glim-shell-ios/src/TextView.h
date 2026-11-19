#pragma once

#import <UIKit/UIKit.h>

#import "TextInputBridge.h"

// UIKit's text input client for the focused Glim field. It draws nothing:
// the field paints in Glim, and this view holds first responder so the
// keyboard, IME, hardware keys and edit menu talk to it. Positions are
// UTF-16 offsets into the published text.
@interface GlimTextView : UIView <UITextInput>
@property(nonatomic, strong, readonly) GlimTextInput* textInput;
// Publishes the field and takes or gives up first responder with it.
- (void)publish:(glim::shell::TextInputState)state;
@end
