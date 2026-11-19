#include <glim/shell/Clipboard.h>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <wayland-client.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "Display.h"

namespace glim::shell {

namespace {

// Text types in order of preference.
constexpr const char* kTextTypes[] = {"text/plain;charset=utf-8", "UTF8_STRING", "text/plain"};
// How long a read waits for the owning client to write.
constexpr int kReadTimeoutMs = 1000;

// The seat's clipboard: the offer the compositor says holds the selection,
// each offer's types, and our own source while we own the selection.
struct Board {
  wl_data_device* device = nullptr;
  wl_data_offer* selection = nullptr;
  std::unordered_map<wl_data_offer*, std::vector<std::string>> types;
  wl_data_source* source = nullptr;
  std::string owned;
};

thread_local Board gBoard;

void dropOffer(wl_data_offer* offer) {
  if (!offer) return;
  gBoard.types.erase(offer);
  wl_data_offer_destroy(offer);
}

void offerType(void*, wl_data_offer* offer, const char* type) {
  gBoard.types[offer].emplace_back(type);
}
void offerActions(void*, wl_data_offer*, uint32_t) {}
void offerAction(void*, wl_data_offer*, uint32_t) {}

const wl_data_offer_listener kOfferListener = [] {
  wl_data_offer_listener l{};
  l.offer = offerType;
  l.source_actions = offerActions;
  l.action = offerAction;
  return l;
}();

void deviceOffer(void*, wl_data_device*, wl_data_offer* offer) {
  wl_data_offer_add_listener(offer, &kOfferListener, nullptr);
}
// Drag and drop is not ours to take; its offers are let go at once.
void deviceEnter(
  void*, wl_data_device*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t, wl_data_offer* offer
) {
  dropOffer(offer);
}
void deviceLeave(void*, wl_data_device*) {}
void deviceMotion(void*, wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t) {}
void deviceDrop(void*, wl_data_device*) {}
void deviceSelection(void*, wl_data_device*, wl_data_offer* offer) {
  if (gBoard.selection != offer) dropOffer(gBoard.selection);
  gBoard.selection = offer;
}

const wl_data_device_listener kDeviceListener = [] {
  wl_data_device_listener l{};
  l.data_offer = deviceOffer;
  l.enter = deviceEnter;
  l.leave = deviceLeave;
  l.motion = deviceMotion;
  l.drop = deviceDrop;
  l.selection = deviceSelection;
  return l;
}();

void sourceTarget(void*, wl_data_source*, const char*) {}
void sourceSend(void*, wl_data_source*, const char*, int32_t fd) {
  const std::string& text = gBoard.owned;
  for (std::size_t at = 0; at < text.size();) {
    const ssize_t n = write(fd, text.data() + at, text.size() - at);
    if (n <= 0) break;
    at += static_cast<std::size_t>(n);
  }
  close(fd);
}
void sourceCancelled(void*, wl_data_source* source) {
  if (source == gBoard.source) {
    gBoard.source = nullptr;
    gBoard.owned.clear();
  }
  wl_data_source_destroy(source);
}
void sourceDropped(void*, wl_data_source*) {}
void sourceFinished(void*, wl_data_source*) {}
void sourceAction(void*, wl_data_source*, uint32_t) {}

const wl_data_source_listener kSourceListener = [] {
  wl_data_source_listener l{};
  l.target = sourceTarget;
  l.send = sourceSend;
  l.cancelled = sourceCancelled;
  l.dnd_drop_performed = sourceDropped;
  l.dnd_finished = sourceFinished;
  l.action = sourceAction;
  return l;
}();

bool ensureDevice() {
  detail::Wayland& wl = detail::wayland();
  if (gBoard.device) return true;
  if (!wl.display || !wl.dataDevices || !wl.seat) return false;
  gBoard.device = wl_data_device_manager_get_data_device(wl.dataDevices, wl.seat);
  wl_data_device_add_listener(gBoard.device, &kDeviceListener, nullptr);
  wl_display_roundtrip(wl.display);
  return true;
}

const char* textType(wl_data_offer* offer) {
  const auto found = gBoard.types.find(offer);
  if (found == gBoard.types.end()) return nullptr;
  for (const char* want : kTextTypes) {
    for (const std::string& have : found->second) {
      if (have == want) return want;
    }
  }
  return nullptr;
}

}  // namespace

namespace detail {

void clipboardDisconnect() {
  dropOffer(gBoard.selection);
  if (gBoard.source) wl_data_source_destroy(gBoard.source);
  if (gBoard.device) wl_data_device_destroy(gBoard.device);
  gBoard = {};
}

}  // namespace detail

ClipboardText readClipboard() {
  if (!ensureDevice()) return {};
  // Our own selection: reading it through the compositor would wait on us.
  if (gBoard.source) {
    return {gBoard.owned.empty() ? ClipboardStatus::Empty : ClipboardStatus::Ok, gBoard.owned};
  }
  const char* type = textType(gBoard.selection);
  if (!type) return {ClipboardStatus::Empty, {}};
  int fds[2];
  if (pipe2(fds, O_CLOEXEC) != 0) return {ClipboardStatus::Empty, {}};
  wl_data_offer_receive(gBoard.selection, type, fds[1]);
  close(fds[1]);
  wl_display_flush(detail::wayland().display);
  std::string text;
  pollfd p{fds[0], POLLIN, 0};
  char buf[4096];
  while (poll(&p, 1, kReadTimeoutMs) > 0) {
    const ssize_t n = read(fds[0], buf, sizeof buf);
    if (n <= 0) break;
    text.append(buf, static_cast<std::size_t>(n));
  }
  close(fds[0]);
  return {text.empty() ? ClipboardStatus::Empty : ClipboardStatus::Ok, std::move(text)};
}

ClipboardStatus writeClipboard(std::string_view text) {
  if (!ensureDevice()) return ClipboardStatus::Unsupported;
  detail::Wayland& wl = detail::wayland();
  wl_data_source* source = wl_data_device_manager_create_data_source(wl.dataDevices);
  for (const char* type : kTextTypes) wl_data_source_offer(source, type);
  wl_data_source_add_listener(source, &kSourceListener, nullptr);
  if (gBoard.source) wl_data_source_destroy(gBoard.source);
  gBoard.source = source;
  gBoard.owned = std::string(text);
  wl_data_device_set_selection(gBoard.device, source, wl.inputSerial);
  wl_display_flush(wl.display);
  return ClipboardStatus::Ok;
}

}  // namespace glim::shell
