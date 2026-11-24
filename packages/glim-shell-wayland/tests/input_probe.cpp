// The Wayland text e2e's input (tools/wayland-text-e2e.sh): one virtual
// pointer and an input method, driven by commands on stdin. It prints each
// input method state the compositor applies.
//
//   click <x> <y> <left|right>   on an output of kExtentW x kExtentH
//   preedit <text>   compose <text>, caret at its end
//   commit <text>    commit <text>
//   delete <b> <a>   delete <b> bytes before and <a> after the caret
//
//   ime state active=1 purpose=0 hint=0 cursor=3 anchor=3 surrounding=abc
#include <poll.h>
#include <unistd.h>
#include <wayland-client.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include <linux/input-event-codes.h>

#include "input-method-unstable-v2-client-protocol.h"
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

namespace {

// The output size tools/wayland-text-e2e.sh configures sway with.
constexpr std::uint32_t kExtentW = 800;
constexpr std::uint32_t kExtentH = 600;

struct State {
  bool active = false;
  std::string surrounding;
  std::uint32_t cursor = 0;
  std::uint32_t anchor = 0;
  std::uint32_t hint = 0;
  std::uint32_t purpose = 0;
};

struct Probe {
  wl_seat* seat = nullptr;
  zwp_input_method_manager_v2* manager = nullptr;
  zwp_input_method_v2* method = nullptr;
  zwlr_virtual_pointer_manager_v1* pointers = nullptr;
  zwlr_virtual_pointer_v1* pointer = nullptr;
  std::uint32_t time = 0;
  State pending;
  std::uint32_t serial = 0;
  bool unavailable = false;
};

Probe gProbe;

void activate(void*, zwp_input_method_v2*) {
  gProbe.pending = {};
  gProbe.pending.active = true;
}
void deactivate(void*, zwp_input_method_v2*) { gProbe.pending.active = false; }
void surrounding(
  void*, zwp_input_method_v2*, const char* text, std::uint32_t cursor, std::uint32_t anchor
) {
  gProbe.pending.surrounding = text ? text : "";
  gProbe.pending.cursor = cursor;
  gProbe.pending.anchor = anchor;
}
void cause(void*, zwp_input_method_v2*, std::uint32_t) {}
void contentType(void*, zwp_input_method_v2*, std::uint32_t hint, std::uint32_t purpose) {
  gProbe.pending.hint = hint;
  gProbe.pending.purpose = purpose;
}
void done(void*, zwp_input_method_v2*) {
  ++gProbe.serial;
  const State& s = gProbe.pending;
  std::cout << "ime state active=" << s.active << " purpose=" << s.purpose << " hint=" << s.hint
            << " cursor=" << s.cursor << " anchor=" << s.anchor << " surrounding=" << s.surrounding
            << std::endl;
}
void unavailable(void*, zwp_input_method_v2*) {
  std::cout << "ime unavailable" << std::endl;
  gProbe.unavailable = true;
}

const zwp_input_method_v2_listener kMethodListener = [] {
  zwp_input_method_v2_listener l{};
  l.activate = activate;
  l.deactivate = deactivate;
  l.surrounding_text = surrounding;
  l.text_change_cause = cause;
  l.content_type = contentType;
  l.done = done;
  l.unavailable = unavailable;
  return l;
}();

void global(
  void*, wl_registry* registry, std::uint32_t name, const char* interface, std::uint32_t
) {
  if (std::strcmp(interface, wl_seat_interface.name) == 0 && !gProbe.seat) {
    gProbe.seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
  } else if (std::strcmp(interface, zwp_input_method_manager_v2_interface.name) == 0) {
    gProbe.manager = static_cast<zwp_input_method_manager_v2*>(
      wl_registry_bind(registry, name, &zwp_input_method_manager_v2_interface, 1)
    );
  } else if (std::strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
    gProbe.pointers = static_cast<zwlr_virtual_pointer_manager_v1*>(
      wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1)
    );
  }
}

void click(std::uint32_t x, std::uint32_t y, std::uint32_t button) {
  zwlr_virtual_pointer_v1* p = gProbe.pointer;
  zwlr_virtual_pointer_v1_motion_absolute(p, gProbe.time += 10, x, y, kExtentW, kExtentH);
  zwlr_virtual_pointer_v1_frame(p);
  zwlr_virtual_pointer_v1_button(p, gProbe.time += 10, button, WL_POINTER_BUTTON_STATE_PRESSED);
  zwlr_virtual_pointer_v1_frame(p);
  zwlr_virtual_pointer_v1_button(p, gProbe.time += 10, button, WL_POINTER_BUTTON_STATE_RELEASED);
  zwlr_virtual_pointer_v1_frame(p);
}
void globalRemove(void*, wl_registry*, std::uint32_t) {}

const wl_registry_listener kRegistryListener = [] {
  wl_registry_listener l{};
  l.global = global;
  l.global_remove = globalRemove;
  return l;
}();

bool run(const std::string& line) {
  std::istringstream in(line);
  std::string verb;
  in >> verb;
  std::string rest;
  std::getline(in >> std::ws, rest);
  zwp_input_method_v2* m = gProbe.method;
  if (verb == "click") {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::string button;
    std::istringstream(rest) >> x >> y >> button;
    if (button != "left" && button != "right") {
      std::cerr << "input probe: unknown button " << line << std::endl;
      return false;
    }
    click(x, y, button == "left" ? BTN_LEFT : BTN_RIGHT);
    return true;
  }
  if (verb == "preedit") {
    const auto end = static_cast<std::int32_t>(rest.size());
    zwp_input_method_v2_set_preedit_string(m, rest.c_str(), end, end);
  } else if (verb == "commit") {
    zwp_input_method_v2_commit_string(m, rest.c_str());
  } else if (verb == "delete") {
    std::uint32_t before = 0;
    std::uint32_t after = 0;
    std::istringstream(rest) >> before >> after;
    zwp_input_method_v2_delete_surrounding_text(m, before, after);
  } else {
    std::cerr << "input probe: unknown command " << line << std::endl;
    return false;
  }
  zwp_input_method_v2_commit(m, gProbe.serial);
  return true;
}

}  // namespace

int main() {
  wl_display* display = wl_display_connect(nullptr);
  if (!display) {
    std::cerr << "input probe: no Wayland display" << std::endl;
    return 1;
  }
  wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &kRegistryListener, nullptr);
  wl_display_roundtrip(display);
  if (!gProbe.seat || !gProbe.manager || !gProbe.pointers) {
    std::cerr << "input probe: the compositor offers no input-method-v2 or virtual pointer"
              << std::endl;
    return 1;
  }
  gProbe.pointer =
    zwlr_virtual_pointer_manager_v1_create_virtual_pointer(gProbe.pointers, gProbe.seat);
  gProbe.method = zwp_input_method_manager_v2_get_input_method(gProbe.manager, gProbe.seat);
  zwp_input_method_v2_add_listener(gProbe.method, &kMethodListener, nullptr);
  wl_display_roundtrip(display);
  std::cout << "input ready" << std::endl;

  std::string buffered;
  while (!gProbe.unavailable) {
    wl_display_flush(display);
    pollfd fds[2] = {{wl_display_get_fd(display), POLLIN, 0}, {STDIN_FILENO, POLLIN, 0}};
    if (poll(fds, 2, -1) < 0) return 1;
    if (fds[0].revents & POLLIN) {
      if (wl_display_dispatch(display) < 0) return 1;
    }
    if (fds[1].revents & (POLLIN | POLLHUP)) {
      char chunk[256];
      const ssize_t n = read(STDIN_FILENO, chunk, sizeof chunk);
      if (n <= 0) break;
      buffered.append(chunk, static_cast<std::size_t>(n));
      for (std::size_t eol; (eol = buffered.find('\n')) != std::string::npos;) {
        const std::string line = buffered.substr(0, eol);
        buffered.erase(0, eol + 1);
        if (!line.empty() && !run(line)) return 1;
      }
    }
  }
  wl_display_disconnect(display);
  return gProbe.unavailable ? 1 : 0;
}
