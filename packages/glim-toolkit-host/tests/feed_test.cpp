#include <glim/toolkit/host/Feed.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  std::uint64_t signaled = 0;
  glim::toolkit::host::Feed feed([&] { return signaled; });

  expect(feed.acquire() == 0, "nothing published");
  expect(!feed.publish({0, 1}), "rejects id 0");
  expect(feed.publish({7, 1}), "publish first");
  expect(feed.acquire() == 0, "fence not passed: no buffer yet");
  expect(feed.stats().fenceMisses == 1 && feed.stats().lag == 1, "miss counted, lag 1");

  signaled = 1;
  expect(feed.acquire() == 7, "fence passed: newest ready");
  expect(feed.stats().lag == 0, "caught up");

  feed.publish({8, 2});
  feed.publish({9, 3});
  feed.publish({10, 4});
  signaled = 3;
  expect(feed.acquire() == 9, "newest ready wins, never waits for 10");
  expect(feed.stats().dropped == 1, "8 dropped");
  expect(feed.stats().lag == 1 && feed.stats().maxLag == 1, "lag 1, max 1");
  expect(feed.acquire() == 9, "holds last ready while 10 is in flight");
  expect(feed.stats().fenceMisses == 2, "second miss");
  expect(!feed.publish({11, 3}), "rejects fence going backwards");

  signaled = 4;
  expect(feed.acquire() == 10, "then advances");
  const glim::toolkit::host::FeedStats s = feed.stats();
  expect(s.published == 4 && s.acquired == 4, "published 4, acquired 4 frames");

  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
