#pragma once

#include <optional>
#include <string_view>

#include "glim/shell/TextInput.h"

namespace glim::shell {

// The Cocoa text system's standard command names (key bindings such as
// "moveWordLeftAndModifySelection:") as changes. AppKit sends them through
// doCommandBySelector:, and UIKit key commands use the same names. A
// single-line field reads up and down as its start and end.
inline std::optional<TextInputChange> textCommand(std::string_view name) {
  using O = TextInputOp;
  using U = TextInputUnit;
  struct Entry {
    std::string_view name;
    TextInputOp op;
    TextInputUnit unit;
    bool extend;
  };
  static constexpr Entry kCommands[] = {
    {"deleteBackward:", O::DeleteBackward, U::Character, false},
    {"deleteBackwardByDecomposingPreviousCharacter:", O::DeleteBackward, U::Character, false},
    {"deleteForward:", O::DeleteForward, U::Character, false},
    {"deleteWordBackward:", O::DeleteBackward, U::Word, false},
    {"deleteWordForward:", O::DeleteForward, U::Word, false},
    {"deleteToBeginningOfLine:", O::DeleteBackward, U::Line, false},
    {"deleteToEndOfLine:", O::DeleteForward, U::Line, false},
    {"deleteToBeginningOfParagraph:", O::DeleteBackward, U::Line, false},
    {"deleteToEndOfParagraph:", O::DeleteForward, U::Line, false},
    {"moveLeft:", O::MoveLeft, U::Character, false},
    {"moveRight:", O::MoveRight, U::Character, false},
    {"moveBackward:", O::MoveLeft, U::Character, false},
    {"moveForward:", O::MoveRight, U::Character, false},
    {"moveWordLeft:", O::MoveLeft, U::Word, false},
    {"moveWordRight:", O::MoveRight, U::Word, false},
    {"moveWordBackward:", O::MoveLeft, U::Word, false},
    {"moveWordForward:", O::MoveRight, U::Word, false},
    {"moveToLeftEndOfLine:", O::MoveLeft, U::Line, false},
    {"moveToRightEndOfLine:", O::MoveRight, U::Line, false},
    {"moveToBeginningOfLine:", O::MoveLeft, U::Line, false},
    {"moveToEndOfLine:", O::MoveRight, U::Line, false},
    {"moveToBeginningOfParagraph:", O::MoveLeft, U::Line, false},
    {"moveToEndOfParagraph:", O::MoveRight, U::Line, false},
    {"moveUp:", O::MoveLeft, U::Document, false},
    {"moveDown:", O::MoveRight, U::Document, false},
    {"moveToBeginningOfDocument:", O::MoveLeft, U::Document, false},
    {"moveToEndOfDocument:", O::MoveRight, U::Document, false},
    {"moveLeftAndModifySelection:", O::MoveLeft, U::Character, true},
    {"moveRightAndModifySelection:", O::MoveRight, U::Character, true},
    {"moveBackwardAndModifySelection:", O::MoveLeft, U::Character, true},
    {"moveForwardAndModifySelection:", O::MoveRight, U::Character, true},
    {"moveWordLeftAndModifySelection:", O::MoveLeft, U::Word, true},
    {"moveWordRightAndModifySelection:", O::MoveRight, U::Word, true},
    {"moveWordBackwardAndModifySelection:", O::MoveLeft, U::Word, true},
    {"moveWordForwardAndModifySelection:", O::MoveRight, U::Word, true},
    {"moveToLeftEndOfLineAndModifySelection:", O::MoveLeft, U::Line, true},
    {"moveToRightEndOfLineAndModifySelection:", O::MoveRight, U::Line, true},
    {"moveToBeginningOfLineAndModifySelection:", O::MoveLeft, U::Line, true},
    {"moveToEndOfLineAndModifySelection:", O::MoveRight, U::Line, true},
    {"moveParagraphBackwardAndModifySelection:", O::MoveLeft, U::Line, true},
    {"moveParagraphForwardAndModifySelection:", O::MoveRight, U::Line, true},
    {"moveUpAndModifySelection:", O::MoveLeft, U::Document, true},
    {"moveDownAndModifySelection:", O::MoveRight, U::Document, true},
    {"moveToBeginningOfDocumentAndModifySelection:", O::MoveLeft, U::Document, true},
    {"moveToEndOfDocumentAndModifySelection:", O::MoveRight, U::Document, true},
    {"selectAll:", O::SelectAll, U::Character, false},
    {"selectWord:", O::SelectWord, U::Character, false},
    {"undo:", O::Undo, U::Character, false},
    {"redo:", O::Redo, U::Character, false},
  };
  for (const Entry& e : kCommands) {
    if (e.name == name) return TextInputChange{.op = e.op, .unit = e.unit, .extend = e.extend};
  }
  return std::nullopt;
}

}  // namespace glim::shell
