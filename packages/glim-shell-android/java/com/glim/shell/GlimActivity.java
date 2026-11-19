package com.glim.shell;

import android.app.NativeActivity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.os.Bundle;
import android.view.ViewGroup;

// Every Glim app's activity: the native run loop, plus the Java the
// platform only offers to views (the IME's InputConnection, the clipboard).
public class GlimActivity extends NativeActivity {
  private TextInputView text;

  @Override
  protected void onCreate(Bundle state) {
    super.onCreate(state);
    text = new TextInputView(this);
    addContentView(text, new ViewGroup.LayoutParams(1, 1));
  }

  // The clipboard's text, or null when it holds none.
  public String readClipboard() {
    final ClipData clip = getSystemService(ClipboardManager.class).getPrimaryClip();
    if (clip == null || clip.getItemCount() == 0) return null;
    final CharSequence text = clip.getItemAt(0).coerceToText(this);
    return text == null ? null : text.toString();
  }

  public void writeClipboard(String text) {
    getSystemService(ClipboardManager.class).setPrimaryClip(ClipData.newPlainText("Glim", text));
  }

  // From the native thread: the focused field in UTF-16 units, or inactive.
  public void publishText(boolean active, boolean secure, String value, int selStart, int selEnd) {
    runOnUiThread(() -> text.publish(active, secure, value, selStart, selEnd));
  }
}
