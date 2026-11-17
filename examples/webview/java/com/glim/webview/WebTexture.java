package com.glim.webview;

import android.app.Activity;
import android.app.Presentation;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.Surface;
import android.webkit.JavascriptInterface;
import android.webkit.WebView;

// Texture mode (K86): the page draws into a private virtual display whose
// surface the native side samples; touches come back in display pixels.
public final class WebTexture {
  private final Activity activity;
  private WebView web;
  private long downTime;

  public static int densityDpi(Activity activity) {
    return activity.getResources().getDisplayMetrics().densityDpi;
  }

  public WebTexture(Activity activity, Surface surface, int width, int height, int dpi, String html) {
    this.activity = activity;
    activity.runOnUiThread(() -> {
      DisplayManager displays = activity.getSystemService(DisplayManager.class);
      VirtualDisplay display = displays.createVirtualDisplay("glim-web", width, height, dpi, surface, 0);
      Presentation presentation = new Presentation(activity, display.getDisplay());
      web = new WebView(presentation.getContext());
      web.getSettings().setJavaScriptEnabled(true);
      web.addJavascriptInterface(new Bridge(), "glim");
      web.loadDataWithBaseURL(null, html, "text/html", "utf-8", null);
      presentation.setContentView(web);
      presentation.show();
    });
  }

  public void touch(int action, float x, float y) {
    activity.runOnUiThread(() -> {
      if (web == null) return;
      long now = SystemClock.uptimeMillis();
      if (action == MotionEvent.ACTION_DOWN) downTime = now;
      MotionEvent event = MotionEvent.obtain(downTime, now, action, x, y, 0);
      event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
      web.dispatchTouchEvent(event);
      event.recycle();
    });
  }

  static final class Bridge {
    @JavascriptInterface
    public void ready() {
      Log.i("glim", "webview page ready");
    }

    @JavascriptInterface
    public void tapped(int count) {
      Log.i("glim", "webview tapped " + count);
    }
  }
}
