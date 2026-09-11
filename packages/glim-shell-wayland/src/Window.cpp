#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <glim/shell/Window.h>

#include "Display.h"
#include "WindowRegistry.h"

#include <glim/shell/RunLoop.h>

#include "xdg-shell-client-protocol.h"
#include "viewporter-client-protocol.h"
#ifdef GLIM_HAS_FRACTIONAL_SCALE
#include "fractional-scale-v1-client-protocol.h"
#endif

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace glim::shell {
namespace {

int createMemfd(off_t size) {
    int fd = memfd_create("glim-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
        return -1;
    }
    if (ftruncate(fd, size) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

}  // namespace

struct WindowImpl {
    Window* owner = nullptr;
    Window::EventCallback callback;
    wl_surface* surface = nullptr;
    xdg_surface* xdgSurface = nullptr;
    xdg_toplevel* toplevel = nullptr;
    wp_viewport* viewport = nullptr;
#ifdef GLIM_HAS_FRACTIONAL_SCALE
    wp_fractional_scale_v1* fractional = nullptr;
#endif
    wl_callback* frame = nullptr;
    std::string title = "Glim";
    int logicalW = 720;
    int logicalH = 480;
    int configuredW = 0;
    int configuredH = 0;
    float scale = 1.0f;
    bool mapped = false;
    bool configured = false;
    bool closed = false;

#if GLIM_SOFTWARE
    struct Software {
        std::vector<std::uint8_t> rgba;
        int width = 0;
        int height = 0;
        int fd = -1;
        std::size_t poolSize = 0;
        wl_shm_pool* pool = nullptr;
        wl_buffer* buffer = nullptr;
        std::uint8_t* pixels = nullptr;
        int bufW = 0;
        int bufH = 0;
    } sw;
#endif

    void destroySoftware() {
#if GLIM_SOFTWARE
        if (sw.buffer) {
            wl_buffer_destroy(sw.buffer);
            sw.buffer = nullptr;
        }
        if (sw.pool) {
            wl_shm_pool_destroy(sw.pool);
            sw.pool = nullptr;
        }
        if (sw.pixels && sw.pixels != MAP_FAILED) {
            munmap(sw.pixels, sw.poolSize);
            sw.pixels = nullptr;
        }
        if (sw.fd >= 0) {
            close(sw.fd);
            sw.fd = -1;
        }
        sw.poolSize = 0;
        sw.bufW = sw.bufH = 0;
#endif
    }

    ~WindowImpl() {
        destroySoftware();
        if (frame) {
            wl_callback_destroy(frame);
        }
#ifdef GLIM_HAS_FRACTIONAL_SCALE
        if (fractional) {
            wp_fractional_scale_v1_destroy(fractional);
        }
#endif
        if (viewport) {
            wp_viewport_destroy(viewport);
        }
        if (toplevel) {
            xdg_toplevel_destroy(toplevel);
        }
        if (xdgSurface) {
            xdg_surface_destroy(xdgSurface);
        }
        if (surface) {
            detail::unregisterSurface(surface);
            wl_surface_destroy(surface);
        }
    }

    int drawableW() const {
        return std::max(1, static_cast<int>(std::round(static_cast<float>(logicalW) * scale)));
    }
    int drawableH() const {
        return std::max(1, static_cast<int>(std::round(static_cast<float>(logicalH) * scale)));
    }

    void applyScale() {
        if (!surface) {
            return;
        }
        const int dw = drawableW();
        const int dh = drawableH();
        if (viewport) {
            wp_viewport_set_destination(viewport, logicalW, logicalH);
            wp_viewport_set_source(viewport, wl_fixed_from_int(0), wl_fixed_from_int(0),
                                   wl_fixed_from_int(dw), wl_fixed_from_int(dh));
        } else {
            const int iscale = std::max(1, static_cast<int>(std::lround(scale)));
            wl_surface_set_buffer_scale(surface, iscale);
        }
    }

    void requestFrame();
};

void xdgSurfaceConfigure(void* data, xdg_surface* xdg, uint32_t serial) {
    auto* impl = static_cast<WindowImpl*>(data);
    xdg_surface_ack_configure(xdg, serial);
    impl->configured = true;
    if (impl->configuredW > 0 && impl->configuredH > 0) {
        impl->logicalW = impl->configuredW;
        impl->logicalH = impl->configuredH;
    }
    impl->applyScale();
    if (impl->callback) {
        Event e(EventType::WindowResized);
        e.setSize(impl->logicalW, impl->logicalH);
        impl->callback(e);
    }
}

void xdgToplevelConfigure(void* data, xdg_toplevel*, int32_t width, int32_t height, wl_array*) {
    auto* impl = static_cast<WindowImpl*>(data);
    if (width > 0 && height > 0) {
        impl->configuredW = width;
        impl->configuredH = height;
    }
}

void xdgToplevelClose(void* data, xdg_toplevel*) {
    auto* impl = static_cast<WindowImpl*>(data);
    impl->closed = true;
    if (impl->callback) {
        impl->callback(Event(EventType::WindowClosed));
    }
}

const xdg_surface_listener kXdgSurfaceListener = [] {
    xdg_surface_listener l{};
    l.configure = xdgSurfaceConfigure;
    return l;
}();
const xdg_toplevel_listener kXdgToplevelListener = [] {
    xdg_toplevel_listener l{};
    l.configure = xdgToplevelConfigure;
    l.close = xdgToplevelClose;
    return l;
}();

#ifdef GLIM_HAS_FRACTIONAL_SCALE
void fractionalPreferred(void* data, wp_fractional_scale_v1*, uint32_t scale120) {
    auto* impl = static_cast<WindowImpl*>(data);
    if (scale120 == 0) {
        return;
    }
    impl->scale = static_cast<float>(scale120) / 120.0f;
    impl->applyScale();
    if (impl->callback) {
        Event e(EventType::WindowResized);
        e.setSize(impl->logicalW, impl->logicalH);
        impl->callback(e);
    }
}
const wp_fractional_scale_v1_listener kFractionalListener = [] {
    wp_fractional_scale_v1_listener l{};
    l.preferred_scale = fractionalPreferred;
    return l;
}();
#endif

void frameDone(void* data, wl_callback* cb, uint32_t) {
    auto* impl = static_cast<WindowImpl*>(data);
    wl_callback_destroy(cb);
    if (impl->frame == cb) {
        impl->frame = nullptr;
    }
    impl->requestFrame();
    RunLoop().requestFrame();
}

const wl_callback_listener kFrameListener{frameDone};

void WindowImpl::requestFrame() {
    if (!surface || frame) {
        return;
    }
    frame = wl_surface_frame(surface);
    wl_callback_add_listener(frame, &kFrameListener, this);
    wl_surface_commit(surface);
}

Window::Window() {
    if (!detail::waylandConnect()) {
        return;
    }
    auto* impl = new WindowImpl();
    impl->owner = this;
    auto& wl = detail::wayland();
    impl->surface = wl_compositor_create_surface(wl.compositor);
    impl->xdgSurface = xdg_wm_base_get_xdg_surface(wl.wm, impl->surface);
    xdg_surface_add_listener(impl->xdgSurface, &kXdgSurfaceListener, impl);
    impl->toplevel = xdg_surface_get_toplevel(impl->xdgSurface);
    xdg_toplevel_add_listener(impl->toplevel, &kXdgToplevelListener, impl);
    xdg_toplevel_set_title(impl->toplevel, impl->title.c_str());
    xdg_toplevel_set_app_id(impl->toplevel, "com.lykhonis.glim.hello");
    if (wl.viewporter) {
        impl->viewport = wp_viewporter_get_viewport(wl.viewporter, impl->surface);
    }
#ifdef GLIM_HAS_FRACTIONAL_SCALE
    if (wl.fractional) {
        impl->fractional = wp_fractional_scale_manager_v1_get_fractional_scale(wl.fractional, impl->surface);
        wp_fractional_scale_v1_add_listener(impl->fractional, &kFractionalListener, impl);
    } else
#endif
    {
        impl->scale = static_cast<float>(std::max(1, wl.outputScale));
    }
    detail::registerSurface(impl->surface, this);
    wl_surface_commit(impl->surface);
    window_ = impl;
    view_ = impl->surface;
}

Window::~Window() {
    detail::unregisterShownWindow(this);
    auto* impl = static_cast<WindowImpl*>(window_);
    delete impl;
    window_ = nullptr;
    view_ = nullptr;
}

void Window::show() {
    auto* impl = static_cast<WindowImpl*>(window_);
    if (!impl || !impl->surface) {
        return;
    }
    if (!shown_) {
        shown_ = true;
        detail::registerShownWindow(this);
    }
    impl->mapped = true;
    impl->applyScale();
    wl_surface_commit(impl->surface);
    if (detail::wayland().display) {
        wl_display_roundtrip(detail::wayland().display);
    }
    impl->requestFrame();
}

void Window::hide() {
    shown_ = false;
    detail::unregisterShownWindow(this);
}

void Window::setSize(int width, int height) {
    auto* impl = static_cast<WindowImpl*>(window_);
    if (!impl) {
        return;
    }
    impl->logicalW = std::max(1, width);
    impl->logicalH = std::max(1, height);
    impl->applyScale();
}

void Window::setTitle(const std::string& title) {
    auto* impl = static_cast<WindowImpl*>(window_);
    if (!impl) {
        return;
    }
    impl->title = title;
    if (impl->toplevel) {
        xdg_toplevel_set_title(impl->toplevel, impl->title.c_str());
    }
}

void Window::center() {}

void Window::setEventCallback(EventCallback callback) {
    auto* impl = static_cast<WindowImpl*>(window_);
    if (impl) {
        impl->callback = std::move(callback);
    }
}

Vec2 Window::size() const {
    auto* impl = static_cast<WindowImpl*>(window_);
    if (!impl) {
        return {};
    }
    return {static_cast<float>(impl->logicalW), static_cast<float>(impl->logicalH)};
}

float Window::pixelRatio() const {
    auto* impl = static_cast<WindowImpl*>(window_);
    return impl ? impl->scale : 1.0f;
}

Vec2 Window::drawableSize() const {
    auto* impl = static_cast<WindowImpl*>(window_);
    if (!impl) {
        return {};
    }
    return {static_cast<float>(impl->drawableW()), static_cast<float>(impl->drawableH())};
}

Rect Window::safeArea() const {
    return Rect::fromSize(size());
}

void* Window::nativeView() const {
    return view_;
}

#if GLIM_SOFTWARE
std::uint8_t* Window::mapSoftware(int width, int height) {
    auto* impl = static_cast<WindowImpl*>(window_);
    if (!impl || width <= 0 || height <= 0) {
        return nullptr;
    }
    impl->sw.width = width;
    impl->sw.height = height;
    impl->sw.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    return impl->sw.rgba.data();
}

void Window::presentSoftware() {
    auto* impl = static_cast<WindowImpl*>(window_);
    auto& wl = detail::wayland();
    if (!impl || !impl->surface || !wl.shm || impl->sw.rgba.empty()) {
        return;
    }
    const int w = impl->sw.width;
    const int h = impl->sw.height;
    const std::size_t stride = static_cast<std::size_t>(w) * 4;
    const std::size_t bytes = stride * static_cast<std::size_t>(h);
    if (w != impl->sw.bufW || h != impl->sw.bufH || !impl->sw.buffer) {
        impl->destroySoftware();
        impl->sw.fd = createMemfd(static_cast<off_t>(bytes));
        if (impl->sw.fd < 0) {
            return;
        }
        impl->sw.poolSize = bytes;
        impl->sw.pixels = static_cast<std::uint8_t*>(
            mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, impl->sw.fd, 0));
        if (impl->sw.pixels == MAP_FAILED) {
            impl->sw.pixels = nullptr;
            impl->destroySoftware();
            return;
        }
        impl->sw.pool = wl_shm_create_pool(wl.shm, impl->sw.fd, static_cast<int32_t>(bytes));
        impl->sw.buffer =
            wl_shm_pool_create_buffer(impl->sw.pool, 0, w, h, static_cast<int32_t>(stride),
                                      WL_SHM_FORMAT_ARGB8888);
        impl->sw.bufW = w;
        impl->sw.bufH = h;
    }
    // CPU raster is RGBA; Wayland ARGB8888 on LE is B,G,R,A.
    const std::uint8_t* src = impl->sw.rgba.data();
    std::uint8_t* dst = impl->sw.pixels;
    const int n = w * h;
    for (int i = 0; i < n; ++i) {
        dst[i * 4 + 0] = src[i * 4 + 2];
        dst[i * 4 + 1] = src[i * 4 + 1];
        dst[i * 4 + 2] = src[i * 4 + 0];
        dst[i * 4 + 3] = src[i * 4 + 3];
    }
    impl->applyScale();
    wl_surface_attach(impl->surface, impl->sw.buffer, 0, 0);
    wl_surface_damage(impl->surface, 0, 0, w, h);
    wl_surface_commit(impl->surface);
    detail::waylandFlush();
}
#endif

void Window::dispatch(const Event& event) {
    auto* impl = static_cast<WindowImpl*>(window_);
    if (impl && impl->callback) {
        impl->callback(event);
    }
}

}  // namespace glim::shell
