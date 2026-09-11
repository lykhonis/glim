#include "Display.h"

#include "WindowRegistry.h"

#include <glim/shell/Event.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Window.h>

#include "xdg-shell-client-protocol.h"
#include "viewporter-client-protocol.h"
#ifdef GLIM_HAS_FRACTIONAL_SCALE
#include "fractional-scale-v1-client-protocol.h"
#endif

#include <linux/input-event-codes.h>
#include <unistd.h>
#include <wayland-client.h>

#include <cstring>
#include <unordered_map>

namespace glim::shell::detail {
namespace {

thread_local Wayland gWl;
thread_local std::unordered_map<wl_surface*, Window*> gSurfaces;
thread_local Window* gPointerFocus = nullptr;

Key mapKey(uint32_t code) {
    switch (code) {
        case KEY_ENTER:
            return Key::Enter;
        case KEY_ESC:
            return Key::Escape;
        case KEY_BACKSPACE:
            return Key::Back;
        case KEY_UP:
            return Key::DpadUp;
        case KEY_DOWN:
            return Key::DpadDown;
        case KEY_LEFT:
            return Key::DpadLeft;
        case KEY_RIGHT:
            return Key::DpadRight;
        default:
            return Key::Unknown;
    }
}

PointerButton mapButton(uint32_t btn) {
    switch (btn) {
        case BTN_LEFT:
            return PointerButton::Left;
        case BTN_RIGHT:
            return PointerButton::Right;
        case BTN_MIDDLE:
            return PointerButton::Middle;
        default:
            return PointerButton::None;
    }
}

void xdgPing(void*, xdg_wm_base* wm, uint32_t serial) {
    xdg_wm_base_pong(wm, serial);
}

const xdg_wm_base_listener kWmListener = [] {
    xdg_wm_base_listener l{};
    l.ping = xdgPing;
    return l;
}();

void outputGeometry(void*, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char*,
                    const char*, int32_t) {}
void outputMode(void*, wl_output*, uint32_t, int32_t, int32_t, int32_t) {}
void outputDone(void*, wl_output*) {}
void outputScale(void*, wl_output*, int32_t factor) {
    if (factor > 0) {
        gWl.outputScale = factor;
    }
}

const wl_output_listener kOutputListener = [] {
    wl_output_listener l{};
    l.geometry = outputGeometry;
    l.mode = outputMode;
    l.done = outputDone;
    l.scale = outputScale;
    return l;
}();

void pointerEnter(void*, wl_pointer*, uint32_t, wl_surface* surface, wl_fixed_t x, wl_fixed_t y) {
    gPointerFocus = windowFromSurface(surface);
    if (!gPointerFocus) {
        return;
    }
    Event e(EventType::PointerMove);
    e.setPoint(wl_fixed_to_double(x), wl_fixed_to_double(y));
    gPointerFocus->dispatch(e);
}
void pointerLeave(void*, wl_pointer*, uint32_t, wl_surface*) {
    gPointerFocus = nullptr;
}
void pointerMotion(void*, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
    if (!gPointerFocus) {
        return;
    }
    Event e(EventType::PointerMove);
    e.setPoint(wl_fixed_to_double(x), wl_fixed_to_double(y));
    gPointerFocus->dispatch(e);
}
void pointerButton(void*, wl_pointer*, uint32_t, uint32_t, uint32_t button, uint32_t state) {
    if (!gPointerFocus) {
        return;
    }
    Event e(state == WL_POINTER_BUTTON_STATE_PRESSED ? EventType::PointerDown : EventType::PointerUp);
    e.setButton(mapButton(button));
    gPointerFocus->dispatch(e);
}
void pointerAxis(void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) {}

const wl_pointer_listener kPointerListener = [] {
    wl_pointer_listener l{};
    l.enter = pointerEnter;
    l.leave = pointerLeave;
    l.motion = pointerMotion;
    l.button = pointerButton;
    l.axis = pointerAxis;
    return l;
}();

void keyboardKeymap(void*, wl_keyboard*, uint32_t, int32_t fd, uint32_t) {
    if (fd >= 0) {
        close(fd);
    }
}
void keyboardEnter(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {}
void keyboardLeave(void*, wl_keyboard*, uint32_t, wl_surface*) {}
void keyboardKey(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t key, uint32_t state) {
    Window* target = gPointerFocus;
    if (!target && !shownWindows().empty()) {
        target = shownWindows().front();
    }
    if (!target) {
        return;
    }
    Event e(state == WL_KEYBOARD_KEY_STATE_PRESSED ? EventType::KeyDown : EventType::KeyUp);
    e.setKey(mapKey(key));
    target->dispatch(e);
}
void keyboardModifiers(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}

const wl_keyboard_listener kKeyboardListener = [] {
    wl_keyboard_listener l{};
    l.keymap = keyboardKeymap;
    l.enter = keyboardEnter;
    l.leave = keyboardLeave;
    l.key = keyboardKey;
    l.modifiers = keyboardModifiers;
    return l;
}();

void seatCapabilities(void*, wl_seat* seat, uint32_t caps) {
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !gWl.pointer) {
        gWl.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(gWl.pointer, &kPointerListener, nullptr);
    }
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !gWl.keyboard) {
        gWl.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(gWl.keyboard, &kKeyboardListener, nullptr);
    }
}
void seatName(void*, wl_seat*, const char*) {}

const wl_seat_listener kSeatListener = [] {
    wl_seat_listener l{};
    l.capabilities = seatCapabilities;
    l.name = seatName;
    return l;
}();

void registryGlobal(void*, wl_registry* registry, uint32_t name, const char* interface,
                    uint32_t version) {
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        gWl.compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, version < 4 ? version : 4));
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        gWl.wm = static_cast<xdg_wm_base*>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        xdg_wm_base_add_listener(gWl.wm, &kWmListener, nullptr);
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        gWl.shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        gWl.seat = static_cast<wl_seat*>(
            wl_registry_bind(registry, name, &wl_seat_interface, version < 5 ? version : 5));
        wl_seat_add_listener(gWl.seat, &kSeatListener, nullptr);
    } else if (std::strcmp(interface, wl_output_interface.name) == 0 && !gWl.output) {
        gWl.output = static_cast<wl_output*>(
            wl_registry_bind(registry, name, &wl_output_interface, version < 2 ? version : 2));
        wl_output_add_listener(gWl.output, &kOutputListener, nullptr);
    } else if (std::strcmp(interface, wp_viewporter_interface.name) == 0) {
        gWl.viewporter = static_cast<wp_viewporter*>(
            wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
#ifdef GLIM_HAS_FRACTIONAL_SCALE
    } else if (std::strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0) {
        gWl.fractional = static_cast<wp_fractional_scale_manager_v1*>(
            wl_registry_bind(registry, name, &wp_fractional_scale_manager_v1_interface, 1));
#endif
    }
}
void registryRemove(void*, wl_registry*, uint32_t) {}

const wl_registry_listener kRegistryListener = [] {
    wl_registry_listener l{};
    l.global = registryGlobal;
    l.global_remove = registryRemove;
    return l;
}();

}  // namespace

Wayland& wayland() {
    return gWl;
}

bool waylandConnect() {
    if (gWl.connected) {
        return gWl.display != nullptr;
    }
    gWl.connected = true;
    gWl.display = wl_display_connect(nullptr);
    if (!gWl.display) {
        return false;
    }
    gWl.registry = wl_display_get_registry(gWl.display);
    wl_registry_add_listener(gWl.registry, &kRegistryListener, nullptr);
    wl_display_roundtrip(gWl.display);
    wl_display_roundtrip(gWl.display);
    return gWl.compositor && gWl.wm;
}

void waylandDisconnect() {
    if (!gWl.display) {
        gWl = {};
        return;
    }
#ifdef GLIM_HAS_FRACTIONAL_SCALE
    if (gWl.fractional) {
        wp_fractional_scale_manager_v1_destroy(gWl.fractional);
    }
#endif
    if (gWl.viewporter) {
        wp_viewporter_destroy(gWl.viewporter);
    }
    if (gWl.pointer) {
        wl_pointer_destroy(gWl.pointer);
    }
    if (gWl.keyboard) {
        wl_keyboard_destroy(gWl.keyboard);
    }
    if (gWl.seat) {
        wl_seat_destroy(gWl.seat);
    }
    if (gWl.output) {
        wl_output_destroy(gWl.output);
    }
    if (gWl.shm) {
        wl_shm_destroy(gWl.shm);
    }
    if (gWl.wm) {
        xdg_wm_base_destroy(gWl.wm);
    }
    if (gWl.compositor) {
        wl_compositor_destroy(gWl.compositor);
    }
    if (gWl.registry) {
        wl_registry_destroy(gWl.registry);
    }
    wl_display_disconnect(gWl.display);
    gWl = {};
}

void waylandFlush() {
    if (gWl.display) {
        wl_display_flush(gWl.display);
    }
}

int waylandFd() {
    return gWl.display ? wl_display_get_fd(gWl.display) : -1;
}

void waylandDispatchPending() {
    if (gWl.display) {
        wl_display_dispatch_pending(gWl.display);
    }
}

bool waylandPrepareRead() {
    return gWl.display && wl_display_prepare_read(gWl.display) == 0;
}

void waylandCancelRead() {
    if (gWl.display) {
        wl_display_cancel_read(gWl.display);
    }
}

int waylandReadEvents() {
    return gWl.display ? wl_display_read_events(gWl.display) : -1;
}

void registerSurface(wl_surface* s, Window* w) {
    if (s) {
        gSurfaces[s] = w;
    }
}

void unregisterSurface(wl_surface* s) {
    gSurfaces.erase(s);
}

Window* windowFromSurface(wl_surface* s) {
    auto it = gSurfaces.find(s);
    return it == gSurfaces.end() ? nullptr : it->second;
}

}  // namespace glim::shell::detail
