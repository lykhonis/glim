#include <iostream>
#include <sstream>
#include <string>

#include "Showcase.h"
#include "elements/Button.h"
#include "elements/Card.h"
#include "elements/Veil.h"
#include "elements/Video.h"
#include "glim/toolkit/Toolkit.h"
#include "paint/Bridge.h"
#include "paint/Ops.h"
#include "paint/Overlay.h"
#include "paint/VideoFeed.h"
#ifndef GLIM_GOLDEN
#include "run.h"
#endif

using namespace glim::toolkit;
using namespace glim::toolkit::paint;

// One scrollable bed: labeled sections, top to bottom. Display state
// echo, buttons, inline frosted dialog, video plate, wrapped paragraph,
// eager list, then (separate tree below) the veil modal overlay.
Block showcaseTree(Showcase s) {
  auto o = s.out;
  auto answer = s.answer;
  Block display =
      roll(horizontal(), scrollOffset(State<float>{}), lines(*o));
  auto digit = [&](const char* label, char d) -> Block {
    return button(label, [o, d] { o = *o + d; }, expand());
  };
  auto section = [&](const char* label, Block content) -> Block {
    return column(gap(4), lines(label), std::move(content));
  };
  Block dialog = card(
      glassClear(),
      radius(6),
      gap(8),
      lines("Delete?"),
      row(gap(8),
          button("No", [answer] { answer = 1; }),
          button("Yes", [answer] { answer = 2; }))
  );
  return roll(
      vertical(),
      column(
          gap(12),
          section("Display", std::move(display)),
          section(
              "Buttons",
              row(gap(8),
                  digit("7", '7'),
                  digit("8", '8'),
                  button(
                      "=", [o] { o = "="; }, primary(), expand()
                  ))
          ),
          section("Dialog", std::move(dialog)),
          section("Video", video()),
          section(
              "Wrap",
              lines(
                  "Frosted card over foreign video, all from one tree. "
                  "Wrap keeps words whole, splits long words, and the "
                  "roll below spills past its viewport by design.",
                  wrap()
              )
          ),
          section(
              "List",
              list(
                  8,
                  [](int i) {
                    return lines("item" + std::to_string(i));
                  }
              )
          )
      )
  );
}

Block showcaseModal(Showcase s) {
  auto answer = s.answer;
  Block dialog = card(
      glassClear(),
      radius(6),
      gap(8),
      lines("Delete?"),
      row(gap(8),
          button("No", [answer] { answer = 1; }),
          button("Yes", [answer] { answer = 2; }))
  );
  return stack(
      veil(),
      column(
          gap(8),
          empty(expand()),
          row(gap(0),
              empty(expand()),
              std::move(dialog),
              empty(expand())),
          empty(expand())
      )
  );
}

std::string dumpTree(Block tree, Constraints c) {
  std::ostringstream os;
  Box box = place(tree, c);
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box);
  return os.str();
}

std::string showcaseDump(Showcase s) {
  return dumpTree(showcaseTree(s), {0, 720, 0, 480});
}

std::string showcaseModalDump(Showcase s) {
  return dumpTree(showcaseModal(s), {0, 720, 0, 480});
}

std::string showcaseFocusDump(Showcase s) {
  Block tree = showcaseTree(s);
  Constraints c{0, 720, 0, 480};
  Box box = place(tree, c);
  std::ostringstream os;
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box);
  Box rb = focusRingBox(tree, box, 0);
  os << kFocusRingKind << " [" << rb.x << "," << rb.y << " " << rb.w
     << "x" << rb.h << "]\n";
  return os.str();
}

void paintShowcaseScene(
    glim::paint::Context& ctx, VideoFeed& feed, Showcase s
) {
  Block screen = showcaseTree(s);
  Block modal = showcaseModal(s);
  Box box = place(screen, {0, ctx.size().x, 0, ctx.size().y});
  // Window base is harness chrome, not tree content: the scrim Veil
  // darkens it, the video plate and frosted cards ride above.
  ctx.setFillColor(Tokens::kWindowBg);
  ctx.fill(glim::Rect{{0, 0}, ctx.size()});
  ContextSink sink{ctx};
  paintShowcaseInto(screen, modal, box, sink, feed);
}

#ifndef GLIM_GOLDEN
void recordExample(ExampleFrame& frame) {
  static Store store;
  static Showcase shown{
      store.make<std::string>("12+3"), store.make<int>(0)
  };
  static VideoFeed feed;
  paintShowcaseScene(frame.context, feed, shown);
}
#endif
