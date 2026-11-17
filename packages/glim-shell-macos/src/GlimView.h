#pragma once

#import <AppKit/AppKit.h>

#include <glim/shell/Window.h>

#import "A11yBridge.h"
#import "TextInputBridge.h"

@interface GlimView : NSView <NSWindowDelegate>
@property(nonatomic, strong) GlimA11yBridge* a11yBridge;
@property(nonatomic, strong) GlimTextInput* textInput;
- (void)setEventCallback:(glim::shell::Window::EventCallback)callback;
- (void)dispatchEvent:(const glim::shell::Event&)event;
@end

// Implemented in TextInput.mm.
@interface GlimView (TextInput) <NSTextInputClient>
@end
