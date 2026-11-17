#import <WebKit/WebKit.h>

#include "PageHtml.h"
#include "WebPage.h"

namespace glim::toolkit {

namespace {

// Glim never retains a slot view, so the factory keeps one page alive.
void* makeWebView(host::SeatRequest request) {
  if (request.id != kWebSeat) return nullptr;
  static WKWebView* web = nil;
  if (web == nil) {
    const CGRect frame = CGRectMake(0, 0, request.bounds.size.x, request.bounds.size.y);
    web = [[WKWebView alloc] initWithFrame:frame configuration:[WKWebViewConfiguration new]];
    [web loadHTMLString:@(kPageHtml) baseURL:nil];
  }
  return (__bridge void*)web;
}

// Slot (K86): WKWebView live content cannot be captured fast enough, so it
// composites as a native sibling and keeps its own input, IME and a11y.
class AppleWebPage final : public WebPage {
 public:
  host::SeatViewFactory seatViews() const override { return &makeWebView; }
  const char* mode() const override { return "Slot: WKWebView over the surface"; }
};

}  // namespace

std::unique_ptr<WebPage> makeWebPage() { return std::make_unique<AppleWebPage>(); }

}  // namespace glim::toolkit
