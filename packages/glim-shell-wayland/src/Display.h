#pragma once

struct wl_display;
struct wl_registry;
struct wl_surface;
struct wl_compositor;
struct wl_shm;
struct wl_seat;
struct wl_pointer;
struct wl_keyboard;
struct wl_output;
struct xdg_wm_base;
struct wp_viewporter;
#ifdef GLIM_HAS_FRACTIONAL_SCALE
struct wp_fractional_scale_manager_v1;
#endif

namespace glim::shell {

class Window;

namespace detail {

struct Wayland {
    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    wl_compositor* compositor = nullptr;
    xdg_wm_base* wm = nullptr;
    wl_shm* shm = nullptr;
    wl_seat* seat = nullptr;
    wl_pointer* pointer = nullptr;
    wl_keyboard* keyboard = nullptr;
    wl_output* output = nullptr;
    wp_viewporter* viewporter = nullptr;
#ifdef GLIM_HAS_FRACTIONAL_SCALE
    wp_fractional_scale_manager_v1* fractional = nullptr;
#endif
    int outputScale = 1;
    bool connected = false;
};

Wayland& wayland();
bool waylandConnect();
void waylandDisconnect();
void waylandFlush();
int waylandFd();
void waylandDispatchPending();
bool waylandPrepareRead();
void waylandCancelRead();
int waylandReadEvents();

void registerSurface(wl_surface*, Window*);
void unregisterSurface(wl_surface*);
Window* windowFromSurface(wl_surface*);

}  // namespace detail
}  // namespace glim::shell
