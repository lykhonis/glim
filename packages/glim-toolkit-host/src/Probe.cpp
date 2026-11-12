#include <glim/image/Load.h>
#include <glim/image/Png.h>
#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/paint/Software.h>
#include <glim/toolkit/host/Probe.h>

#if !GLIM_SOFTWARE
#include <glim/gpu/Device.h>
#include <glim/paint/Renderer.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>

#include "Trace.h"

namespace glim {
namespace toolkit {
namespace host {

namespace gi = ::glim::image;
using Clock = std::chrono::steady_clock;

#if !GLIM_SOFTWARE
struct Probe::Gpu {
  gpu::Device device;
  std::unique_ptr<paint::Renderer> renderer;
};
#else
struct Probe::Gpu {};
#endif

namespace {

constexpr const char* kHelp = R"(probe commands (one per line, answers end with ok | err <reason>):
  size W H                      resize the headless app
  out DIR                       artifact directory (default /tmp/glim-probe)
  ref PATH                      load reference image (PNG or JPEG)
  region NAME X Y W H           add a compare region (reference coordinates)
  item NAME X Y W H MASK        add a placement item: ink bounds of MASK inside the rect
  items                         ref vs cur ink bounds per item (dx dy dw dh)
  render [cpu|gpu]              rasterize the current tree; prints timing
  snap PATH [cpu|gpu]           render and write PNG
  compare [LABEL]               score current frame vs ref per region; writes
                                side/heat/onion/regions sheets, appends history.tsv
  history                       print history.tsv
  zoom X Y W H [SCALE] [GRID]   ref|cur|heat crop -> OUT/zoom.png
  ruler ref|cur X Y W H [SCALE] crop with coordinate grid -> OUT/ruler.png
  prof ref|cur row|col AT [FROM TO] [MIN] [raw]   luma peaks along a line
  runs ref|cur row|col AT FROM TO MASK            mask runs along a line
  ray ref|cur CX CY DEG R0 R1 [MIN]               luma peaks by radius along a ray
  ring ref|cur CX CY R MASK [DEG0 DEG1]           mask runs (degrees) along a circle
  bbox ref|cur X Y W H MASK     bounds of masked pixels (orange|bright|dim|#rrggbb[:tol])
  color ref|cur X Y W H [FRAC]  median ink (brightest FRAC) and background
  pick ref|cur X Y              pixel value
  tree | a11y | stats           toolkit dump, a11y nodes, paint stats
  hit X Y                       elements under a point, root to leaf
  ops                           op counts by kind and per element
  bench [N]                     median build/record/encode/raster ms over N frames
  encode                        encode cost by paint shape kind (count, instances, ms)
  tap X Y | tap i N | wheel X Y DX DY | step DT   input and time
  key Left|Right|Up|Down|Next|Tab|Prev|Activate|Enter|Space
  expect TEXT                   assert TEXT appears in tree or a11y dump
  help)";

std::optional<Key> e2eKey(const std::string& s) {
  if (s == "Left") return Key::Left;
  if (s == "Right") return Key::Right;
  if (s == "Up") return Key::Up;
  if (s == "Down") return Key::Down;
  if (s == "Next" || s == "Tab") return Key::Next;
  if (s == "Prev") return Key::Prev;
  if (s == "Activate" || s == "Enter" || s == "Space") return Key::Activate;
  return std::nullopt;
}

std::vector<std::string> split(std::string_view line) {
  std::vector<std::string> out;
  std::istringstream is{std::string(line)};
  std::string w;
  while (is >> w) out.push_back(w);
  return out;
}

std::string restAfter(std::string_view line, std::size_t words) {
  std::size_t i = 0;
  for (std::size_t n = 0; n < words; ++n) {
    i = line.find_first_not_of(" \t", i);
    if (i == std::string_view::npos) return {};
    i = line.find_first_of(" \t", i);
    if (i == std::string_view::npos) return {};
  }
  const std::size_t b = line.find_first_not_of(" \t", i);
  return b == std::string_view::npos ? std::string() : std::string(line.substr(b));
}

std::string expandPath(const std::string& p) {
  const char* home = std::getenv("HOME");
  if (p.size() >= 1 && p[0] == '~' && home) return home + p.substr(1);
  return p;
}

int toInt(const std::vector<std::string>& a, std::size_t i, int fallback) {
  return i < a.size() ? std::atoi(a[i].c_str()) : fallback;
}

float toFloat(const std::vector<std::string>& a, std::size_t i, float fallback) {
  return i < a.size() ? static_cast<float>(std::atof(a[i].c_str())) : fallback;
}

gi::Region regionAt(const std::vector<std::string>& a, std::size_t i) {
  return {toInt(a, i, 0), toInt(a, i + 1, 0), toInt(a, i + 2, 0), toInt(a, i + 3, 0)};
}

std::string hex(gi::Rgb c) {
  char b[16];
  std::snprintf(b, sizeof(b), "#%02x%02x%02x", c.r, c.g, c.b);
  return b;
}

double median(std::vector<double> v) { return detail::percentile(std::move(v), 50); }

double msSince(Clock::time_point t) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

struct Scores {
  double mad = 0, edge = 0;
};

// history.tsv row: time label overall edge name=mad/edge ...
std::map<std::string, Scores> lastHistory(const std::string& path, Scores* overall) {
  std::ifstream in(path);
  std::string line, last;
  while (std::getline(in, line))
    if (!line.empty()) last = line;
  std::map<std::string, Scores> out;
  std::istringstream is(last);
  std::string time, label, cell;
  if (!std::getline(is, time, '\t') || !std::getline(is, label, '\t')) return out;
  std::string o, e;
  if (std::getline(is, o, '\t') && std::getline(is, e, '\t'))
    *overall = {std::atof(o.c_str()), std::atof(e.c_str())};
  while (std::getline(is, cell, '\t')) {
    const auto eq = cell.find('='), sl = cell.find('/');
    if (eq == std::string::npos || sl == std::string::npos) continue;
    out[cell.substr(0, eq)] = {
        std::atof(cell.substr(eq + 1, sl - eq - 1).c_str()), std::atof(cell.substr(sl + 1).c_str())
    };
  }
  return out;
}

std::string delta(double cur, double old, bool have) {
  if (!have || std::fabs(cur - old) < 0.005) return "";
  char b[32];
  std::snprintf(b, sizeof(b), " (%+.2f)", cur - old);
  return b;
}

}  // namespace

Probe::Probe(TestApp& app, std::ostream& out) : app_(app), out_(out) {}
Probe::~Probe() = default;

std::string Probe::artifact(const std::string& name) const {
  std::error_code ec;
  std::filesystem::create_directories(outDir_, ec);
  return outDir_ + "/" + name;
}

bool Probe::render(bool gpu) {
  const auto t0 = Clock::now();
  if (!app_.laid) app_.rebuild();
  paint::Context ctx;
  app_.recordScene(ctx);
  const double recordMs = msSince(t0);
  const int w = static_cast<int>(app_.w), h = static_cast<int>(app_.h);
  frame_.pixels = gi::Image::rgba8(w, h);
  frame_.gpu = gpu;
  const auto t1 = Clock::now();
  if (!gpu) {
    paint::rasterScene(ctx.scene(), w, h, frame_.pixels.rgba.data());
  } else {
#if !GLIM_SOFTWARE
    if (!gpu_) {
      gpu::DeviceCreateInfo info{};
#if defined(__APPLE__)
      info.backend = gpu::Backend::Metal;
#else
      info.backend = gpu::Backend::Vulkan;
#endif
      auto created = gpu::Device::create(info);
      if (!created.ok()) {
        out_ << "gpu device: " << created.message() << "\n";
        return false;
      }
      gpu_ = std::make_unique<Gpu>();
      gpu_->device = std::move(created.value());
      gpu_->renderer = std::make_unique<paint::Renderer>(gpu_->device);
    }
    auto target = gpu_->device.createFrameTarget({w, h});
    if (!target.ok()) {
      out_ << "gpu target: " << target.message() << "\n";
      return false;
    }
    gpu_->renderer->drawToTarget(ctx.scene(), target.value());
    if (!gpu_->device.readPixels(
            target.value(), frame_.pixels.rgba.data(), frame_.pixels.rgba.size()
        )) {
      out_ << "gpu readPixels unsupported on this backend\n";
      return false;
    }
#else
    out_ << "gpu unavailable in a SOFTWARE build\n";
    return false;
#endif
  }
  frame_.valid = true;
  out_ << std::fixed << std::setprecision(2) << "render backend=" << (gpu ? "gpu" : "cpu")
       << " record_ms=" << recordMs << " raster_ms=" << msSince(t1);
#if !GLIM_SOFTWARE
  // readPixels waited for the frame, so the device's latest is this one.
  if (gpu && gpu_ && gpu_->device.lastGpuMs() > 0.f) {
    out_ << " gpu_ms=" << gpu_->device.lastGpuMs();
  }
#endif
  out_ << "\n";
  return true;
}

const Probe::Frame& Probe::current() {
  if (!frame_.valid) render(false);
  return frame_;
}

const gi::Image* Probe::pick(const std::string& which) {
  if (which == "ref") return ref_.empty() ? nullptr : &ref_;
  if (which == "cur") return &current().pixels;
  return nullptr;
}

bool Probe::compare(const std::string& label) {
  const gi::Image& cur = current().pixels;
  if (ref_.empty()) return out_ << "no ref\n", false;
  if (ref_.width != cur.width || ref_.height != cur.height)
    return out_ << "size mismatch ref " << ref_.width << "x" << ref_.height << " cur " << cur.width
                << "x" << cur.height << "\n",
           false;
  const std::string hist = artifact("history.tsv");
  Scores lastAll;
  const auto last = lastHistory(hist, &lastAll);
  const bool haveLast = !last.empty() || lastAll.mad > 0;
  std::ostringstream row;
  std::time_t now = std::time(nullptr);
  char stamp[32];
  std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
  const Scores all{gi::mad(ref_, cur), gi::edgeMad(ref_, cur)};
  row << stamp << '\t' << (label.empty() ? "-" : label) << '\t' << all.mad << '\t' << all.edge;
  out_ << std::fixed << std::setprecision(2);
  std::vector<gi::Image> sheetRows;
  for (const auto& [name, r] : regions_) {
    const gi::Image a = gi::crop(ref_, r), b = gi::crop(cur, r);
    const Scores s{gi::mad(a, b), gi::edgeMad(a, b)};
    const auto it = last.find(name);
    const bool have = it != last.end();
    out_ << "region " << name << " mad=" << s.mad << delta(s.mad, have ? it->second.mad : 0, have)
         << " edge=" << s.edge << delta(s.edge, have ? it->second.edge : 0, have) << "\n";
    row << '\t' << name << '=' << s.mad << '/' << s.edge;
    const int scale = std::clamp(180 / std::max(1, r.h), 1, 4);
    sheetRows.push_back(gi::stack(
        {gi::scaleNearest(a, scale),
         gi::scaleNearest(b, scale),
         gi::scaleNearest(gi::heat(a, b), scale)},
        false,
        4
    ));
  }
  out_ << "overall mad=" << all.mad << delta(all.mad, lastAll.mad, haveLast) << " edge=" << all.edge
       << delta(all.edge, lastAll.edge, haveLast) << " over2=" << gi::compare(ref_, cur).overDelta
       << "\n";
  std::ofstream(hist, std::ios::app) << row.str() << "\n";
  gi::Png::write(artifact("side.png").c_str(), gi::stack({ref_, cur}, false, 0));
  gi::Png::write(artifact("heat.png").c_str(), gi::heat(ref_, cur));
  gi::Png::write(artifact("onion.png").c_str(), gi::blend(ref_, cur, 0.5f));
  if (!sheetRows.empty())
    gi::Png::write(artifact("regions.png").c_str(), gi::stack(sheetRows, true, 6));
  out_ << "sheets " << outDir_ << "/{side,heat,onion,regions}.png\n";
  return true;
}

bool Probe::bench(int frames) {
  frames = std::max(1, frames);
  std::vector<double> build, record, encode, raster;
  const int w = static_cast<int>(app_.w), h = static_cast<int>(app_.h);
  gi::Image px = gi::Image::rgba8(w, h);
  std::size_t instances = 0;
  for (int i = 0; i < frames; ++i) {
    auto t = Clock::now();
    app_.rebuild();
    build.push_back(msSince(t));
    paint::Context ctx;
    t = Clock::now();
    app_.recordScene(ctx);
    record.push_back(msSince(t));
    t = Clock::now();
    instances = paint::encode(ctx.scene(), 1.f).stats.instances;
    encode.push_back(msSince(t));
    t = Clock::now();
    paint::rasterScene(ctx.scene(), w, h, px.rgba.data());
    raster.push_back(msSince(t));
  }
  out_ << std::fixed << std::setprecision(3) << "bench frames=" << frames
       << " build_ms=" << median(build) << " record_ms=" << median(record)
       << " encode_ms=" << median(encode) << " cpu_raster_ms=" << median(raster)
       << " instances=" << instances << "\n";
  return true;
}

bool Probe::dispatch(const std::vector<std::string>& a, const std::string& line) {
  const std::string& cmd = a[0];
  auto invalidate = [&] { frame_.valid = false; };
  if (cmd == "help") return out_ << kHelp << "\n", true;
  if (cmd == "size" && a.size() >= 3) {
    app_.w = toFloat(a, 1, app_.w), app_.h = toFloat(a, 2, app_.h);
    app_.rebuild();
    return invalidate(), true;
  }
  if (cmd == "out" && a.size() >= 2) return outDir_ = expandPath(a[1]), true;
  if (cmd == "ref" && a.size() >= 2) {
    if (!gi::load(expandPath(a[1]).c_str(), &ref_))
      return out_ << "cannot load " << a[1] << "\n", false;
    return out_ << "ref " << ref_.width << "x" << ref_.height << "\n", true;
  }
  if (cmd == "region" && a.size() >= 6) {
    regions_.emplace_back(a[1], regionAt(a, 2));
    return true;
  }
  if (cmd == "item" && a.size() >= 7) {
    Item it{a[1], regionAt(a, 2), {}};
    if (!gi::parseMask(a[6], &it.mask)) return out_ << "bad mask " << a[6] << "\n", false;
    items_.push_back(std::move(it));
    return true;
  }
  if (cmd == "items") {
    if (ref_.empty()) return out_ << "no ref\n", false;
    const gi::Image& cur = current().pixels;
    double err = 0;
    int n = 0;
    for (const Item& it : items_) {
      const gi::Bounds r = gi::maskBounds(ref_, it.search, it.mask);
      const gi::Bounds c = gi::maskBounds(cur, it.search, it.mask);
      out_ << "item " << it.name;
      if (!r.count || !c.count) {
        out_ << (r.count ? " cur=none" : " ref=none") << "\n";
        continue;
      }
      const int dx = c.x0 - r.x0, dy = c.y0 - r.y0;
      const int dw = (c.x1 - c.x0) - (r.x1 - r.x0), dh = (c.y1 - c.y0) - (r.y1 - r.y0);
      err += std::abs(dx) + std::abs(dy) + std::abs(dw) + std::abs(dh), ++n;
      out_ << " ref=" << r.x0 << "," << r.y0 << "," << (r.x1 - r.x0 + 1) << "x" << (r.y1 - r.y0 + 1)
           << " dx=" << dx << " dy=" << dy << " dw=" << dw << " dh=" << dh << "\n";
    }
    out_ << std::fixed << std::setprecision(2) << "items matched=" << n
         << " mean_abs_px=" << (n ? err / (4.0 * n) : 0.0) << "\n";
    return true;
  }
  if (cmd == "render") return render(a.size() > 1 && a[1] == "gpu");
  if (cmd == "snap" && a.size() >= 2) {
    if (!render(a.size() > 2 && a[2] == "gpu")) return false;
    return gi::Png::write(expandPath(a[1]).c_str(), frame_.pixels) ||
           (out_ << "cannot write\n", false);
  }
  if (cmd == "compare") return compare(restAfter(line, 1));
  if (cmd == "history") {
    std::ifstream in(artifact("history.tsv"));
    std::string l;
    while (std::getline(in, l)) {
      std::istringstream is(l);
      std::string t, lab, o, e;
      std::getline(is, t, '\t'), std::getline(is, lab, '\t');
      std::getline(is, o, '\t'), std::getline(is, e, '\t');
      out_ << t << " overall=" << o << " edge=" << e << " " << lab << "\n";
    }
    return true;
  }
  if (cmd == "zoom" && a.size() >= 5) {
    const gi::Region r = regionAt(a, 1);
    const int s = toInt(a, 5, 3), grid = toInt(a, 6, 0);
    if (ref_.empty()) return out_ << "no ref\n", false;
    gi::Image pa = gi::crop(ref_, r), pb = gi::crop(current().pixels, r);
    const double m = gi::mad(pa, pb);
    std::vector<gi::Image> parts{
        gi::scaleNearest(pa, s), gi::scaleNearest(pb, s), gi::scaleNearest(gi::heat(pa, pb), s)
    };
    if (grid > 0)
      for (gi::Image& p : parts) gi::drawRuler(p, r, s, grid);
    const bool vertical = r.w > r.h * 3 / 2;
    const std::string path = artifact("zoom.png");
    gi::Png::write(path.c_str(), gi::stack(parts, vertical, 4));
    out_ << "zoom " << path << (vertical ? " ref/cur/heat stacked" : " ref|cur|heat")
         << " mad=" << std::fixed << std::setprecision(2) << m << "\n";
    return true;
  }
  if (cmd == "ruler" && a.size() >= 6) {
    const gi::Image* im = pick(a[1]);
    if (!im) return out_ << "no " << a[1] << "\n", false;
    const gi::Region r = regionAt(a, 2);
    const int s = toInt(a, 6, 3);
    gi::Image z = gi::scaleNearest(gi::crop(*im, r), s);
    gi::drawRuler(z, r, s, 10);
    const std::string path = artifact("ruler.png");
    gi::Png::write(path.c_str(), z);
    return out_ << "ruler " << path << "\n", true;
  }
  if ((cmd == "prof" || cmd == "runs") && a.size() >= 4) {
    const gi::Image* im = pick(a[1]);
    if (!im) return out_ << "no " << a[1] << "\n", false;
    gi::Line l{a[2] == "row", toInt(a, 3, 0), toInt(a, 4, 0), toInt(a, 5, -1)};
    if (a.size() < 6) l.from = 0, l.to = -1;
    if (cmd == "runs") {
      gi::Mask m;
      if (a.size() < 7 || !gi::parseMask(a[6], &m)) return out_ << "bad mask\n", false;
      out_ << "runs";
      for (const gi::Run& r : gi::maskRuns(*im, l, m)) out_ << " " << r.from << ".." << r.to;
      return out_ << "\n", true;
    }
    out_ << "peaks";
    for (const gi::Peak& p : gi::lumaPeaks(*im, l, toInt(a, 6, 40)))
      out_ << " " << p.at << ":" << p.luma;
    out_ << "\n";
    if (a.back() == "raw") {
      int i = std::max(0, l.from);
      for (int v : gi::lumaLine(*im, l)) out_ << i++ << ":" << v << " ";
      out_ << "\n";
    }
    return true;
  }
  if (cmd == "ray" && a.size() >= 7) {
    const gi::Image* im = pick(a[1]);
    if (!im) return out_ << "no " << a[1] << "\n", false;
    out_ << "ray";
    for (const gi::Peak& p : gi::rayPeaks(
             *im,
             toFloat(a, 2, 0),
             toFloat(a, 3, 0),
             toFloat(a, 4, 0),
             toInt(a, 5, 0),
             toInt(a, 6, 0),
             toInt(a, 7, 40)
         ))
      out_ << " r" << p.at << ":" << p.luma;
    return out_ << "\n", true;
  }
  if (cmd == "ring" && a.size() >= 6) {
    const gi::Image* im = pick(a[1]);
    gi::Mask m;
    if (!im || !gi::parseMask(a[5], &m)) return out_ << "bad image or mask\n", false;
    out_ << std::fixed << std::setprecision(2) << "ring";
    for (const gi::ArcRun& r : gi::ringRuns(
             *im,
             toFloat(a, 2, 0),
             toFloat(a, 3, 0),
             toFloat(a, 4, 0),
             m,
             toFloat(a, 6, 0),
             toFloat(a, 7, 360)
         ))
      out_ << " " << r.fromDeg << ".." << r.toDeg;
    return out_ << "\n", true;
  }
  if (cmd == "bbox" && a.size() >= 7) {
    const gi::Image* im = pick(a[1]);
    gi::Mask m;
    if (!im || !gi::parseMask(a[6], &m)) return out_ << "bad image or mask\n", false;
    const gi::Bounds b = gi::maskBounds(*im, regionAt(a, 2), m);
    if (!b.count) return out_ << "bbox none\n", true;
    out_ << std::fixed << std::setprecision(1) << "bbox x=" << b.x0 << ".." << b.x1 << " y=" << b.y0
         << ".." << b.y1 << " n=" << b.count << " centroid=" << b.cx << "," << b.cy << "\n";
    return true;
  }
  if (cmd == "color" && a.size() >= 6) {
    const gi::Image* im = pick(a[1]);
    if (!im) return out_ << "no " << a[1] << "\n", false;
    const gi::InkStats s =
        gi::inkStats(*im, regionAt(a, 2), a.size() > 6 ? std::atof(a[6].c_str()) : 0.08);
    return out_ << "color ink=" << hex(s.ink) << " bg=" << hex(s.background) << "\n", true;
  }
  if (cmd == "pick" && a.size() >= 4) {
    const gi::Image* im = pick(a[1]);
    const int x = toInt(a, 2, 0), y = toInt(a, 3, 0);
    if (!im || x < 0 || y < 0 || x >= im->width || y >= im->height)
      return out_ << "out of range\n", false;
    return out_ << "pick " << hex(gi::pixel(*im, x, y)) << "\n", true;
  }
  if (cmd == "tree" || cmd == "a11y" || cmd == "stats") {
    const auto s = app_.snapshotTimed(false);
    out_ << (cmd == "tree" ? s.dump : cmd == "a11y" ? s.a11y : app_.dumpStats(s) + "\n");
    return true;
  }
  if (cmd == "hit" && a.size() >= 3) {
    const float x = toFloat(a, 1, 0), y = toFloat(a, 2, 0);
    if (!app_.laid) app_.rebuild();
    Rec rec;
    record(app_.tree, app_.box, rec, app_.engine());
    for (const Rec::Entry& en : rec.entries) {
      const Box& b = en.box;
      if (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) continue;
      out_ << en.kind << " box=" << b.x << "," << b.y << "," << b.w << "x" << b.h;
      if (!en.text.empty()) out_ << " text=\"" << en.text << "\"";
      if (!en.mark.empty()) out_ << " mark=" << en.mark;
      out_ << "\n";
    }
    return true;
  }
  if (cmd == "ops") {
    const auto s = app_.snapshotTimed(false);
    std::map<std::string, int> byKind;
    std::vector<std::pair<std::string, int>> byElement;
    for (const Op& op : s.ops) {
      ++byKind[opKindName(op.kind)];
      if (op.kind == Op::Kind::Entry)
        byElement.emplace_back(op.entry.kind, 0);
      else if (!byElement.empty())
        ++byElement.back().second;
    }
    out_ << "ops total=" << s.ops.size() << "\n";
    for (const auto& [k, n] : byKind) out_ << "kind " << k << "=" << n << "\n";
    for (const auto& [k, n] : byElement)
      if (n > 0) out_ << "element " << k << " ops=" << n << "\n";
    return true;
  }
  if (cmd == "tops") {
    // Toolkit op stream only (no paint encode): exact Push/Pop order.
    app_.rebuild();
    OpsRecorder rec;
    {
      Engine e = app_.engine();
      paintTree(app_.tree, app_.box, e, rec, app_.feed);
    }
    auto isPush = [](Op::Kind k) {
      return k == Op::Kind::PushGlass || k == Op::Kind::PushEffect || k == Op::Kind::PushBlend ||
             k == Op::Kind::PushClip || k == Op::Kind::PushShadow || k == Op::Kind::PushTransform ||
             k == Op::Kind::PushRotate || k == Op::Kind::PushGlass;
    };
    int depth = 0;
    for (const Op& op : rec.ops) {
      if (op.kind == Op::Kind::Pop) depth = std::max(0, depth - 1);
      if (op.kind == Op::Kind::Entry || isPush(op.kind) || op.kind == Op::Kind::Pop)
        out_ << std::string(static_cast<std::size_t>(depth), ' ') << opKindName(op.kind)
             << (op.kind == Op::Kind::Entry ? std::string(" ") + op.entry.kind : "") << "\n";
      if (isPush(op.kind)) ++depth;
    }
    return true;
  }
  if (cmd == "bench") return bench(toInt(a, 1, 10));
  if (cmd == "encode") {
    paint::Context ctx;
    app_.recordScene(ctx);
    const auto t = Clock::now();
    const paint::Stats st = paint::encode(ctx.scene(), 1.f).stats;
    out_ << std::fixed << std::setprecision(2) << "encode ms=" << msSince(t)
         << " instances=" << st.instances << " isolates=" << st.isolateCount << "\n";
    for (std::size_t k = 0; k < paint::kShapeKinds; ++k)
      if (st.shapeCount[k])
        out_ << "shape " << paint::shapeKindName(k) << " count=" << st.shapeCount[k]
             << " instances=" << st.shapeInstances[k] << " ms=" << st.shapeEncodeMs[k] << "\n";
    return true;
  }
  if (cmd == "tap" && a.size() >= 3 && a[1] == "i")
    return app_.tapIndex(toInt(a, 2, 0)), invalidate(), true;
  if (cmd == "tap" && a.size() >= 3)
    return app_.tap(toFloat(a, 1, 0), toFloat(a, 2, 0)), invalidate(), true;
  if (cmd == "key" && a.size() >= 2) {
    const auto k = e2eKey(a[1]);
    if (!k) return out_ << "unknown key: " << a[1] << "\n", false;
    return app_.key(*k), invalidate(), true;
  }
  if (cmd == "wheel" && a.size() >= 5) {
    app_.wheel(toFloat(a, 1, 0), toFloat(a, 2, 0), toFloat(a, 3, 0), toFloat(a, 4, 0));
    return invalidate(), true;
  }
  if (cmd == "step" && a.size() >= 2) return app_.step(toFloat(a, 1, 0)), invalidate(), true;
  if (cmd == "expect") {
    const std::string want = restAfter(line, 1);
    // Text only: no raster needed to find a string.
    const auto s = app_.snapshotTimed(false);
    if ((s.dump + "\n" + s.a11y).find(want) == std::string::npos)
      return out_ << "expect miss: " << want << "\n", false;
    return true;
  }
  return out_ << "unknown or incomplete command: " << line << "\n", false;
}

bool Probe::exec(std::string_view raw) {
  std::string line(raw);
  if (const auto c = line.find('#'); c == 0) return true;
  const std::vector<std::string> args = split(line);
  if (args.empty()) return true;
  const bool ok = dispatch(args, line);
  out_ << (ok ? "ok" : "err") << "\n";
  out_.flush();
  return ok;
}

bool Probe::run(std::istream& in, bool stopOnError) {
  bool all = true;
  std::string line;
  while (std::getline(in, line)) {
    if (line == "quit" || line == "exit") break;
    if (!exec(line)) {
      all = false;
      if (stopOnError) break;
    }
  }
  return all;
}

E2eFlags parseE2e(int argc, char** argv) {
  E2eFlags f;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&](const char* p) -> std::string {
      std::string q = p;
      if (a == q && i + 1 < argc) return argv[++i];
      if (a.rfind(q + "=", 0) == 0) return a.substr(q.size() + 1);
      return "";
    };
    auto has = [&](const char* p) -> bool {
      std::string q = p;
      return a == q || a.rfind(q + "=", 0) == 0;
    };
    if (a == "--help" || a == "-h")
      f.help = true;
    else if (has("--e2e"))
      f.script = val("--e2e");
    else if (has("--probe"))
      f.probe = val("--probe");
    else if (has("--snapshot"))
      f.snapshot = val("--snapshot");
    else if (has("--dump-ops"))
      f.dumpOps = val("--dump-ops");
    else if (a == "--dump-a11y")
      f.dumpA11y = true;
    else if (a == "--stats")
      f.stats = true;
    else if (has("--trace-frames")) {
      std::string v = val("--trace-frames");
      f.traceFrames = v.empty() ? 60 : std::atoi(v.c_str());
    } else if (has("--trace-out"))
      f.traceOut = val("--trace-out");
    else if (has("--trace-json"))
      f.traceJson = val("--trace-json");
    else if (has("--soak")) {
      std::string v = val("--soak");
      f.soak = v.empty() ? 60 : std::atoi(v.c_str());
    } else if (has("--drive-trace"))
      f.driveTrace = val("--drive-trace");
    else if (has("--drive"))
      f.drive = val("--drive");
    else if (a == "--startup")
      f.startup = StartupUntil::Presented;
    else if (a == "--startup-encoded")
      f.startup = StartupUntil::Encoded;
  }
  return f;
}

int runE2e(TestApp& app, const E2eFlags& f) {
  app.rebuild();
  if (!f.probe.empty()) {
    Probe probe(app, std::cout);
    if (f.probe == "-") return probe.run(std::cin, false) ? 0 : 1;
    std::ifstream in(f.probe);
    if (!in) return std::cerr << "cannot open probe script " << f.probe << "\n", 1;
    return probe.run(in, true) ? 0 : 1;
  }
  {
    // Token script answers stay off stdout (it may carry --dump-a11y); shown on failure.
    std::ostringstream answers;
    Probe probe(app, answers);
    std::size_t pos = 0;
    while (pos <= f.script.size()) {
      const auto semi = f.script.find(';', pos);
      const std::string tok =
          f.script.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);
      pos = semi == std::string::npos ? f.script.size() + 1 : semi + 1;
      const auto b = tok.find_first_not_of(" \t\r\n");
      if (b == std::string::npos) continue;
      const std::string t = tok.substr(b, tok.find_last_not_of(" \t\r\n") - b + 1);
      if (!probe.exec(probeLineFromE2e(t))) {
        std::cerr << "e2e step failed: " << t << "\n" << answers.str();
        return 1;
      }
    }
  }
  if (f.soak > 0) {
    // Soak: repeat tap/wheel/step, assert ops stable + values settle.
    // Wheels cancel out so scroll returns home; any op drift is growth.
    // No raster (fast) — ops + revision only.
    auto base = app.snapshotTimed(false);
    size_t baseOps = base.ops.size();
    for (int i = 0; i < f.soak; ++i) {
      app.tap(8 + (i % 5) * 10, 8);
      app.wheel(500, 300, 0, -20);
      app.step(0.016f);
      app.wheel(500, 300, 0, 20);
      app.step(0.016f);
    }
    auto cur = app.snapshotTimed(false);
    if (cur.ops.size() != baseOps) {
      std::cerr << "soak ops drift: base " << baseOps << " cur " << cur.ops.size() << "\n";
      return 1;
    }
    if (app.store.valuesLive()) {
      std::cerr << "soak values still live after settle\n";
      return 1;
    }
  }
  if (f.traceFrames > 0) {
    std::ostream* out = &std::cout;
    std::ofstream file;
    if (!f.traceOut.empty()) {
      file.open(f.traceOut);
      if (!file) return 1;
      out = &file;
    }
    std::ofstream jfile;
    std::optional<detail::TraceWriter> trace;
    if (!f.traceJson.empty()) {
      jfile.open(f.traceJson);
      if (!jfile) return 1;
      trace.emplace(jfile);
    }
    *out << "frame,buildMs,recordMs,encodeMs,ops,rev\n";
    for (int i = 0; i < f.traceFrames; ++i) {
      app.step(0.016f);
      auto s = app.snapshotTimed(false);
      *out << i << "," << s.buildMs << "," << s.recordMs << "," << s.encodeMs << "," << s.ops.size()
           << "," << s.revision << "\n";
      if (trace) {
        // One 60 Hz slot per frame, phases laid end to end.
        long long ts = static_cast<long long>(i) * 16666;
        for (const auto& [name, ms] :
             {std::pair{"build", s.buildMs},
              std::pair{"record", s.recordMs},
              std::pair{"encode", s.encodeMs}}) {
          const auto dur = static_cast<long long>(ms * 1000.0f);
          trace->slice(name, 1, ts, dur);
          ts += dur;
        }
      }
    }
    if (trace) trace->close();
  }
  if (!f.snapshot.empty()) {
    auto s = app.snapshotTimed(false);
    if (f.stats) std::cout << app.dumpStats(s) << "\n";
    if (!app.writePng(f.snapshot)) return 1;
  } else if (f.stats) {
    std::cout << app.dumpStats(app.snapshotTimed(false)) << "\n";
  }
  if (!f.dumpOps.empty() && !app.writeOps(f.dumpOps)) return 1;
  if (f.dumpA11y) std::cout << app.snapshotTimed(false).a11y;
  return 0;
}

std::string probeLineFromE2e(std::string_view token) {
  if (!token.empty() && token[0] == '~') return "expect " + std::string(token.substr(1));
  std::string s(token);
  const auto colon = s.find(':');
  if (colon == std::string::npos) return s;
  std::string verb = s.substr(0, colon), rest = s.substr(colon + 1);
  if (verb == "snap") return "snap " + rest;
  for (char& c : rest)
    if (c == ':' || c == ',') c = ' ';
  return verb + " " + rest;
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
