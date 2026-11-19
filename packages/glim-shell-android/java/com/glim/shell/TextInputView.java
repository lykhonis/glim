package com.glim.shell;

import android.content.Context;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

// The IME's client for the focused Glim field. It draws nothing: the field
// paints in Glim. Edits go to native code, which applies them to the field
// and publishes the result back.
final class TextInputView extends View {
  private boolean active;
  private boolean secure;
  private String value = "";
  private int selStart;
  private int selEnd;

  TextInputView(Context context) {
    super(context);
    setFocusable(true);
    setFocusableInTouchMode(true);
  }

  void publish(boolean active, boolean secure, String value, int selStart, int selEnd) {
    final boolean opened = active && !this.active;
    final boolean closed = !active && this.active;
    this.active = active;
    this.secure = secure;
    this.value = value;
    this.selStart = selStart;
    this.selEnd = selEnd;
    final InputMethodManager imm = getContext().getSystemService(InputMethodManager.class);
    if (opened) {
      requestFocus();
      imm.restartInput(this);
      imm.showSoftInput(this, 0);
    } else if (closed) {
      imm.hideSoftInputFromWindow(getWindowToken(), 0);
      clearFocus();
    } else if (active) {
      imm.updateSelection(this, selStart, selEnd, -1, -1);
    }
  }

  @Override
  public boolean onCheckIsTextEditor() {
    return active;
  }

  @Override
  public InputConnection onCreateInputConnection(EditorInfo info) {
    if (!active) return null;
    info.inputType = InputType.TYPE_CLASS_TEXT
        | (secure ? InputType.TYPE_TEXT_VARIATION_PASSWORD : InputType.TYPE_TEXT_FLAG_AUTO_CORRECT);
    info.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_FLAG_NO_EXTRACT_UI;
    info.initialSelStart = selStart;
    info.initialSelEnd = selEnd;
    return new Connection();
  }

  private final class Connection extends BaseInputConnection {
    Connection() {
      super(TextInputView.this, false);
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
      nativeCompose(text.toString());
      return true;
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
      nativeCommit(text.toString());
      return true;
    }

    @Override
    public boolean finishComposingText() {
      nativeFinishComposing();
      return true;
    }

    @Override
    public boolean setSelection(int start, int end) {
      nativeSelect(start, end);
      return true;
    }

    @Override
    public boolean deleteSurroundingText(int before, int after) {
      nativeDeleteSurrounding(Math.max(0, before), Math.max(0, after));
      return true;
    }

    @Override
    public boolean sendKeyEvent(KeyEvent event) {
      if (event.getAction() != KeyEvent.ACTION_DOWN) return true;
      if (event.getKeyCode() == KeyEvent.KEYCODE_DEL) {
        nativeDeleteBackward();
      } else if (event.getKeyCode() == KeyEvent.KEYCODE_FORWARD_DEL) {
        nativeDeleteForward();
      } else if (event.getUnicodeChar() != 0) {
        nativeCommit(String.valueOf((char) event.getUnicodeChar()));
      }
      return true;
    }

    @Override
    public CharSequence getTextBeforeCursor(int n, int flags) {
      final int a = Math.min(selStart, selEnd);
      return secure ? "" : value.substring(Math.max(0, a - n), a);
    }

    @Override
    public CharSequence getTextAfterCursor(int n, int flags) {
      final int b = Math.max(selStart, selEnd);
      return secure ? "" : value.substring(b, Math.min(value.length(), b + n));
    }

    @Override
    public CharSequence getSelectedText(int flags) {
      if (secure || selStart == selEnd) return null;
      return value.substring(Math.min(selStart, selEnd), Math.max(selStart, selEnd));
    }
  }

  private static native void nativeCompose(String text);
  private static native void nativeCommit(String text);
  private static native void nativeFinishComposing();
  private static native void nativeSelect(int start, int end);
  private static native void nativeDeleteSurrounding(int before, int after);
  private static native void nativeDeleteBackward();
  private static native void nativeDeleteForward();
}
