#include <iostream>
#include <sstream>
#include <string>

#include "elements/Button.h"
#include "elements/Card.h"
#include "elements/Veil.h"
#include "elements/Video.h"
#include "glim/toolkit/Toolkit.h"
#include "paint/Bridge.h"
#include "paint/Demo.h"
#include "paint/Ops.h"
#include "paint/VideoFeed.h"
#ifndef GLIM_GOLDEN
#include <glim/image/Image.h>
#include <glim/image/Png.h>
#include <glim/paint/Software.h>

#include <functional>
#include <type_traits>
#endif

using namespace glim::toolkit;
using namespace glim::toolkit::paint;

Block calcTree(State<std::string> out) {
  auto o = out;
  Block display =
      roll(horizontal(), scrollOffset(State<float>{}), lines(*o));
  auto digit = [&](const char* label, char d) -> Block {
    return button(label, [o, d] { o = *o + d; }, expand());
  };
  auto op = [&](const char* label) -> Block {
    return button(label, [o, label] { o = *o + label; }, expand());
  };
  return column(
      gap(8),
      display,
      row(gap(8),
          digit("7", '7'),
          digit("8", '8'),
          digit("9", '9'),
          op("/")),
      row(gap(8),
          digit("4", '4'),
          digit("5", '5'),
          digit("6", '6'),
          op("*")),
      row(gap(8),
          digit("1", '1'),
          digit("2", '2'),
          digit("3", '3'),
          button(
              "=", [o] { o = "="; }, primary(), expand()
          ))
  );
}

std::string dumpTree(Block tree, Constraints c) {
  std::ostringstream os;
  Box box = place(tree, c);
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree, box);
  return os.str();
}

std::string calcDump() {
  Store store;
  return dumpTree(
      calcTree(store.make<std::string>("12+3")), {0, 720, 0, 480}
  );
}

Block modalTree(Action onNo, Action onYes) {
  Block dialog = card(
      radius(6),
      gap(8),
      lines("Delete?"),
      row(gap(8),
          button("No", std::move(onNo)),
          button("Yes", std::move(onYes)))
  );
  return stack(
      empty(expand()),
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

std::string modalDump() {
  return dumpTree(modalTree([] {}, [] {}), {0, 720, 0, 480});
}

std::string listDump() {
  Block tree = roll(vertical(), list(8, [](int i) {
                      return lines("item" + std::to_string(i));
                    }));
  return dumpTree(std::move(tree), {0, 720, 0, 100});
}

std::string focusDump() {
  Store store;
  Block tree = calcTree(store.make<std::string>("12+3"));
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

// T4 paint demo: scrim + foreign-video plate under a frosted card,
// seat strip as a sibling after (never inside) the glass group.
// Same trees as the dump goldens plus Veil/Video/glass — the dump
// goldens above stay byte-stable; this tree is paint-only (see Demo.h).

void paintDemoScene(glim::paint::Context& ctx, VideoFeed& feed) {
  Block demo = paintDemoTree([] {}, [] {});
  Box box = place(demo, {0, ctx.size().x, 0, ctx.size().y});
  // Window base is harness chrome, not tree content: the scrim Veil
  // darkens it, the video plate and frosted card ride above.
  ctx.setFillColor(Tokens::kWindowBg);
  ctx.fill(glim::Rect{{0, 0}, ctx.size()});
  ContextSink sink{ctx};
  paintDemoInto(demo, box, sink, feed);
}

#ifndef GLIM_GOLDEN
int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--paint") {
    glim::paint::Context ctx;
    ctx.setSize({720, 480});
    ctx.beginFrame();
    VideoFeed feed;
    paintDemoScene(ctx, feed);
    ctx.finish();
    auto img = glim::image::Image::rgba8(720, 480);
    glim::paint::rasterScene(ctx.scene(), 720, 480, img.rgba.data());
    const char* path = argc > 2 ? argv[2] : "/tmp/calc-paint.png";
    if (!glim::image::Png::write(path, img)) {
      std::cerr << "could not write " << path << '\n';
      return 1;
    }
    std::cout << "wrote " << path << '\n';
    if (argc > 3 && std::string(argv[3]) == "--debug") {
      std::function<void(const glim::paint::Group&, int)> dump =
          [&](const glim::paint::Group& g, int depth) {
            std::string pad(static_cast<std::size_t>(depth * 2), ' ');
            if (g.params.glass)
              std::cout << pad << "Group glass="
                        << (g.params.glass->variant ==
                                    glim::paint::GlassVariant::Clear
                                ? "clear"
                                : "regular")
                        << '\n';
            glim::paint::visitGroup(
                g,
                [&](const glim::paint::Shape& s) {
                  std::visit(
                      [&](const auto& sh) {
                        using T = std::decay_t<decltype(sh)>;
                        if constexpr (std::is_same_v<
                                          T,
                                          glim::paint::FillRounded>)
                          std::cout
                              << pad << " FillRounded ["
                              << sh.rect.origin.x << ","
                              << sh.rect.origin.y << " "
                              << sh.rect.size.x << "x" << sh.rect.size.y
                              << "] r=" << sh.radius.lt << " rgba=0x"
                              << std::hex << sh.matter.color.rgba
                              << std::dec << '\n';
                        else if constexpr (std::is_same_v<
                                               T,
                                               glim::paint::FillRect>)
                          std::cout << pad << " FillRect ["
                                    << sh.rect.origin.x << ","
                                    << sh.rect.origin.y << "]\n";
                        else if constexpr (std::is_same_v<
                                               T,
                                               glim::paint::Blit>)
                          std::cout
                              << pad << " Blit [" << sh.rect.origin.x
                              << "," << sh.rect.origin.y << " "
                              << sh.rect.size.x << "x" << sh.rect.size.y
                              << "]\n";
                        else if constexpr (std::is_same_v<
                                               T,
                                               glim::paint::GlyphRun>)
                          std::cout << pad << " GlyphRun n="
                                    << sh.glyphs.size() << '\n';
                        else if constexpr (std::is_same_v<
                                               T,
                                               glim::paint::SlotHole>)
                          std::cout << pad << " SlotHole id=" << sh.id
                                    << '\n';
                        else if constexpr (std::is_same_v<
                                               T,
                                               glim::paint::Stroke>)
                          std::cout << pad << " Stroke w=" << sh.width
                                    << '\n';
                      },
                      s
                  );
                },
                [&](const glim::paint::Group& c) { dump(c, depth + 1); }
            );
          };
      dump(ctx.scene().root, 0);
    }
    return 0;
  }
  std::cout << calcDump();
  return 0;
}
#endif
