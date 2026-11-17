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
import android.graphics.Bitmap;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;

// Texture mode (K86): the page draws into a private virtual display whose
// surface the native side samples; touches come back in display pixels.
public final class WebTexture {
  // The bundled page's address; the native side shares it.
  static final String LOCAL = "about:glim";

  private final Activity activity;
  private final String html;
  private WebView web;
  private long downTime;
  // Written on the UI thread, read by the native frame: the version moves
  // on every change so the native side fetches strings only then.
  private volatile int version;
  private volatile String url = LOCAL;
  private volatile String title = "";
  private volatile int progress;
  private volatile boolean loading = true;
  private boolean local = true;

  public static int densityDpi(Activity activity) {
    return activity.getResources().getDisplayMetrics().densityDpi;
  }

  public WebTexture(Activity activity, Surface surface, int width, int height, int dpi, String html) {
    this.activity = activity;
    this.html = html;
    activity.runOnUiThread(() -> {
      DisplayManager displays = activity.getSystemService(DisplayManager.class);
      VirtualDisplay display = displays.createVirtualDisplay("glim-web", width, height, dpi, surface, 0);
      Presentation presentation = new Presentation(activity, display.getDisplay());
      web = new WebView(presentation.getContext());
      web.getSettings().setJavaScriptEnabled(true);
      web.addJavascriptInterface(new Bridge(), "glim");
      web.setWebViewClient(new WebViewClient() {
        @Override
        public void onPageStarted(WebView view, String at, Bitmap icon) {
          loading = true;
          changed();
        }

        @Override
        public void onPageFinished(WebView view, String at) {
          url = local ? LOCAL : at;
          title = view.getTitle() == null ? "" : view.getTitle();
          loading = false;
          changed();
        }
      });
      web.setWebChromeClient(new WebChromeClient() {
        @Override
        public void onProgressChanged(WebView view, int percent) {
          progress = percent;
          changed();
        }

        @Override
        public void onReceivedTitle(WebView view, String text) {
          title = text == null ? "" : text;
          changed();
        }
      });
      showLocal();
      presentation.setContentView(web);
      presentation.show();
    });
  }

  private void changed() {
    version = version + 1;
  }

  private void showLocal() {
    local = true;
    web.loadDataWithBaseURL(null, html, "text/html", "utf-8", null);
  }

  public void load(String address) {
    activity.runOnUiThread(() -> {
      if (web == null) return;
      if (LOCAL.equals(address)) {
        showLocal();
      } else {
        local = false;
        web.loadUrl(address);
      }
    });
  }

  public void reload() {
    activity.runOnUiThread(() -> {
      if (web == null) return;
      if (local) {
        showLocal();
      } else {
        web.reload();
      }
    });
  }

  public int version() {
    return version;
  }

  public String url() {
    return url;
  }

  public String title() {
    return title;
  }

  public int progress() {
    return progress;
  }

  public boolean loading() {
    return loading;
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
