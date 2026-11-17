#import <WebKit/WebKit.h>

#include "PageHtml.h"
#include "WebPage.h"

namespace glim::toolkit {

namespace {

WKWebView* webView() {
  static WKWebView* web = nil;
  if (web == nil) {
    web = [[WKWebView alloc] initWithFrame:CGRectZero configuration:[WKWebViewConfiguration new]];
    [web loadHTMLString:@(kPageHtml) baseURL:nil];
  }
  return web;
}

// Glim never retains a slot view; the page lives for the process.
void* makeWebView(host::SeatRequest request) {
  if (request.id != kWebSeat) return nullptr;
  return (__bridge void*)webView();
}

std::string str(NSString* s) { return s.UTF8String ? std::string(s.UTF8String) : std::string(); }

// Slot, and Layer when tilted (K88): WKWebView has no public GPU capture, so
// it stays a live native view. Flat it is a sibling over the surface;
// under a 3D transform Core Animation composites it projected and the
// shell routes its input. WebKit keeps its own IME and VoiceOver tree.
class AppleWebPage final : public WebPage {
 public:
  host::SeatViewFactory seatViews() const override { return &makeWebView; }
  const char* mode() const override { return "WKWebView: native layer"; }

  bool advance() override {
    WebStatus next = read();
    if (next == status_) return false;
    status_ = std::move(next);
    return true;
  }

  WebStatus status() const override { return status_; }

  void load(const std::string& url) override {
    local_ = url == kLocalUrl;
    if (local_) {
      [webView() loadHTMLString:@(kPageHtml) baseURL:nil];
      return;
    }
    NSURL* target = [NSURL URLWithString:@(url.c_str())];
    if (target) [webView() loadRequest:[NSURLRequest requestWithURL:target]];
  }

  void reload() override {
    if (local_) {
      load(kLocalUrl);
    } else {
      [webView() reload];
    }
  }

 private:
  WebStatus read() const {
    WKWebView* web = webView();
    return {
      .url = local_ ? std::string(kLocalUrl) : str(web.URL.absoluteString),
      .title = str(web.title),
      .progress = static_cast<float>(web.estimatedProgress),
      .loading = static_cast<bool>(web.isLoading),
    };
  }

  bool local_ = true;
  WebStatus status_;
};

}  // namespace

std::unique_ptr<WebPage> makeWebPage() { return std::make_unique<AppleWebPage>(); }

}  // namespace glim::toolkit
