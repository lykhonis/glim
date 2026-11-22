#include <glim/shell/Application.h>
#include <glim/shell/Surface.h>
#include <glim/shell/Window.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

int main() {
  glim::shell::Window window;
  window.setSize(720, 480);
  if (window.size().x != 720.f || window.size().y != 480.f) {
    std::cerr << "custom size mismatch\n";
    return EXIT_FAILURE;
  }
  if (window.drawableSize().x != 720.f || window.safeArea().size.x != 720.f) {
    std::cerr << "custom drawable/safe mismatch\n";
    return EXIT_FAILURE;
  }
  window.setPixelRatio(2.f);
  if (window.drawableSize().x != 1440.f || window.pixelRatio() != 2.f) {
    std::cerr << "custom pixelRatio mismatch\n";
    return EXIT_FAILURE;
  }
  window.setPixelRatio(1.f);

  int frames = 0;
  int resized = 0;
  window.setEventCallback([&](const glim::shell::Event& e) {
    if (e.type() == glim::shell::EventType::Frame) {
      ++frames;
    }
    if (e.type() == glim::shell::EventType::WindowResized) {
      ++resized;
    }
  });
  window.show();
  window.injectFrame();
  glim::shell::Event rs(glim::shell::EventType::WindowResized);
  window.inject(rs);
  if (frames != 1 || resized != 1) {
    std::cerr << "custom inject mismatch\n";
    return EXIT_FAILURE;
  }

  // Text entry: the embedder reads the focused field and sends it edits
  // (typing and navigation, no IME); nothing reaches a field that is not
  // focused.
  std::vector<glim::shell::TextInputChange> edits;
  window.setTextInputCallback([&](const glim::shell::TextInputChange& c) { edits.push_back(c); });
  if (window.injectText({.op = glim::shell::TextInputOp::Insert, .text = "a"}) || !edits.empty()) {
    std::cerr << "custom text reached no field\n";
    return EXIT_FAILURE;
  }
  window.setTextInput({.active = true, .text = "ab", .selection = {2, 0}});
  if (!window.textInput().active || window.textInput().text != "ab") {
    std::cerr << "custom text state mismatch\n";
    return EXIT_FAILURE;
  }
  if (!window.injectText({.op = glim::shell::TextInputOp::Insert, .text = "c"}) ||
      !window.injectText(
        {.op = glim::shell::TextInputOp::MoveLeft, .unit = glim::shell::TextInputUnit::Word}
      ) ||
      edits.size() != 2 || edits[0].text != "c" ||
      edits[1].op != glim::shell::TextInputOp::MoveLeft) {
    std::cerr << "custom text edits mismatch\n";
    return EXIT_FAILURE;
  }
  window.setTextInput({});
  if (window.injectText({.op = glim::shell::TextInputOp::DeleteBackward}) || edits.size() != 2) {
    std::cerr << "custom text after blur\n";
    return EXIT_FAILURE;
  }

  glim::shell::SlotNative native{};
  window.attachSlot(7, native);
  window.positionSlot(7, glim::Rect{{10.f, 10.f}, {100.f, 40.f}});
  if (window.slotCount() != 1) {
    std::cerr << "custom slot mismatch\n";
    return EXIT_FAILURE;
  }
  window.detachSlot(7);
  if (window.slotCount() != 0) {
    std::cerr << "custom slot detach mismatch\n";
    return EXIT_FAILURE;
  }

  glim::shell::Surface surface;
  surface.attach(window);
  surface.setVSync(true);
  surface.setNative(0, reinterpret_cast<void*>(0x1));
  auto info = surface.deviceCreateInfo();
  if (info.native[0] != reinterpret_cast<void*>(0x1)) {
    std::cerr << "custom surface native mismatch\n";
    return EXIT_FAILURE;
  }
  if (surface.drawableSize().x != 720.f || surface.pixelRatio() != 1.f) {
    std::cerr << "custom surface size mismatch\n";
    return EXIT_FAILURE;
  }

#if GLIM_SOFTWARE
  std::uint8_t* px = window.mapSoftware(720, 480);
  if (px == nullptr) {
    std::cerr << "custom mapSoftware null\n";
    return EXIT_FAILURE;
  }
  px[0] = 1;
  window.presentSoftware();
#endif

  window.hide();
  glim::shell::Application app;
  app.run();

  std::cout << "custom_smoke ok\n";
  return EXIT_SUCCESS;
}
