#include "WebGpuImpl.h"

namespace glim::gpu::webgpu {

WebBootstrap& web() {
  static WebBootstrap s;
  return s;
}

namespace {

void onDeviceLost(
    const WGPUDevice* device,
    WGPUDeviceLostReason reason,
    WGPUStringView message,
    void* userdata1,
    void* userdata2
) {
  (void)device;
  (void)reason;
  (void)message;
  (void)userdata1;
  (void)userdata2;
  WebBootstrap& w = web();
  printf("[glim-web] device lost reason=%d\n", (int)reason);
  w.ready = false;
  w.deviceRequested = false;
  w.device = nullptr;
  w.queue = nullptr;
}

void onDevice(
    WGPURequestDeviceStatus status,
    WGPUDevice device,
    WGPUStringView message,
    void* userdata1,
    void* userdata2
) {
  (void)userdata1;
  (void)userdata2;
  WebBootstrap& w = web();
  if (status != WGPURequestDeviceStatus_Success || device == nullptr) {
    w.failed = true;
    w.error = "webgpu requestDevice failed";
    if (message.data != nullptr && message.length != WGPU_STRLEN && message.length > 0) {
      w.error += ": ";
      w.error += std::string(message.data, message.length);
    }
    return;
  }
  w.device = device;
  w.queue = wgpuDeviceGetQueue(device);
  WGPUSurfaceCapabilities caps{};
  if (wgpuSurfaceGetCapabilities(w.surface, w.adapter, &caps) != WGPUStatus_Success ||
      caps.formatCount == 0) {
    w.failed = true;
    w.error = "webgpu surface has no formats";
    return;
  }
  w.format = caps.formats[0];
  wgpuSurfaceCapabilitiesFreeMembers(caps);
  w.ready = true;
}

void onUncaptured(
    const WGPUDevice* device,
    WGPUErrorType type,
    WGPUStringView message,
    void* userdata1,
    void* userdata2
) {
  (void)device;
  (void)type;
  (void)userdata1;
  (void)userdata2;
  if (message.data != nullptr && message.length > 0 && message.length != WGPU_STRLEN) {
    printf("[glim-web] uncaptured: %.*s\n", static_cast<int>(message.length), message.data);
  } else if (message.data != nullptr) {
    printf("[glim-web] uncaptured: %s\n", message.data);
  }
}

}  // namespace

void requestDevice() {
  WebBootstrap& w = web();
  if (w.adapter == nullptr || w.deviceRequested || w.device != nullptr) {
    return;
  }
  WGPUDeviceDescriptor dd{};
  dd.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
  dd.deviceLostCallbackInfo.callback = onDeviceLost;
  dd.uncapturedErrorCallbackInfo.callback = onUncaptured;
  WGPURequestDeviceCallbackInfo cb{};
  cb.mode = WGPUCallbackMode_AllowSpontaneous;
  cb.callback = onDevice;
  w.deviceRequested = true;
  wgpuAdapterRequestDevice(w.adapter, &dd, cb);
}

namespace {

void onAdapter(
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    WGPUStringView message,
    void* userdata1,
    void* userdata2
) {
  (void)userdata1;
  (void)userdata2;
  WebBootstrap& w = web();
  if (status != WGPURequestAdapterStatus_Success || adapter == nullptr) {
    w.failed = true;
    w.error = "webgpu requestAdapter failed";
    if (message.data != nullptr && message.length != WGPU_STRLEN && message.length > 0) {
      w.error += ": ";
      w.error += std::string(message.data, message.length);
    }
    return;
  }
  w.adapter = adapter;
  requestDevice();
}

}  // namespace

void ensureInstance(const char* selector) {
  WebBootstrap& w = web();
  if (w.instance != nullptr) {
    return;
  }
  if (selector != nullptr && selector[0] != '\0') {
    w.selector = selector;
  }
  w.instance = wgpuCreateInstance(nullptr);
  if (w.instance == nullptr) {
    w.failed = true;
    w.error = "wgpuCreateInstance failed";
    return;
  }
  WGPUEmscriptenSurfaceSourceCanvasHTMLSelector sel{};
  sel.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
  sel.selector = strView(w.selector.c_str(), w.selector.size());
  WGPUSurfaceDescriptor sd{};
  sd.nextInChain = &sel.chain;
  w.surface = wgpuInstanceCreateSurface(w.instance, &sd);
  if (w.surface == nullptr) {
    w.failed = true;
    w.error = "webgpu surface creation failed";
  }
}

void ensureAdapter() {
  WebBootstrap& w = web();
  if (w.adapterRequested || w.surface == nullptr) {
    return;
  }
  WGPURequestAdapterOptions opts{};
  opts.compatibleSurface = w.surface;
  WGPURequestAdapterCallbackInfo cb{};
  cb.mode = WGPUCallbackMode_AllowSpontaneous;
  cb.callback = onAdapter;
  w.adapterRequested = true;
  wgpuInstanceRequestAdapter(w.instance, &opts, cb);
}

}  // namespace glim::gpu::webgpu
