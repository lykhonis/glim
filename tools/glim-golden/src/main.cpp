#include <glim/image/Analyze.h>
#include <glim/image/Image.h>
#include <glim/image/Load.h>
#include <glim/image/Png.h>

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace image = glim::image;

namespace {

constexpr int kBlankColors = 10;

struct Options {
  std::vector<std::string> positional;
  image::Region crop{};
  bool hasCrop = false;
  int tolerance = 2;
  long maxOver = 0;
  int maxDelta = 0;
  std::string save;
};

bool parseRegion(const std::string& s, image::Region* out) {
  return std::sscanf(s.c_str(), "%d,%d,%d,%d", &out->x, &out->y, &out->w, &out->h) == 4;
}

bool parse(int argc, char** argv, Options* o) {
  for (int i = 2; i < argc; ++i) {
    const std::string a = argv[i];
    const bool more = i + 1 < argc;
    if (a == "--crop" && more) {
      if (!parseRegion(argv[++i], &o->crop)) return false;
      o->hasCrop = true;
    } else if (a == "--tolerance" && more) {
      o->tolerance = std::atoi(argv[++i]);
    } else if (a == "--max-over" && more) {
      o->maxOver = std::atol(argv[++i]);
    } else if (a == "--max-delta" && more) {
      o->maxDelta = std::atoi(argv[++i]);
    } else if (a == "--save" && more) {
      o->save = argv[++i];
    } else if (a.rfind("--", 0) == 0) {
      return false;
    } else {
      o->positional.push_back(a);
    }
  }
  return true;
}

bool open(const std::string& path, const Options& o, image::Image* out) {
  image::Image full;
  if (!image::load(path.c_str(), &full)) {
    std::cerr << "cannot read " << path << "\n";
    return false;
  }
  *out = o.hasCrop ? image::crop(full, o.crop) : full;
  return true;
}

int distinctColors(const image::Image& im) {
  std::set<unsigned> colors;
  for (std::size_t i = 0; i + 3 < im.rgba.size(); i += 4) {
    colors.insert(im.rgba[i] << 16 | im.rgba[i + 1] << 8 | im.rgba[i + 2]);
    if (static_cast<int>(colors.size()) >= kBlankColors) break;
  }
  return static_cast<int>(colors.size());
}

// blank IMAGE [--crop]: exit 0 when drawn, 3 when blank (too few colors).
int blank(const Options& o) {
  image::Image im;
  if (o.positional.size() != 1 || !open(o.positional[0], o, &im)) return 2;
  const bool isBlank = distinctColors(im) < kBlankColors;
  std::cout << (isBlank ? "blank" : "ready") << "\n";
  return isBlank ? 3 : 0;
}

// compare GOT GOLDEN [--crop] [--tolerance N] --max-over N --max-delta N
// [--save FILE]: exit 0 within limits, 1 on mismatch.
int compare(const Options& o) {
  image::Image got, want;
  if (o.positional.size() != 2 || !open(o.positional[0], o, &got)) return 2;
  if (!image::load(o.positional[1].c_str(), &want)) {
    std::cerr << "cannot read " << o.positional[1] << "\n";
    return 2;
  }
  if (!o.save.empty() && !image::Png::write(o.save.c_str(), got)) return 2;
  const image::Diff diff = image::compare(got, want, o.tolerance);
  if (!diff.sameSize) {
    std::cout << "size mismatch " << got.width << "x" << got.height << " vs " << want.width << "x"
              << want.height << "\n";
    return 1;
  }
  const bool ok = static_cast<long>(diff.overPixels) <= o.maxOver && diff.maxDelta <= o.maxDelta;
  std::cout << "over-delta-" << o.tolerance << "=" << diff.overPixels << " (limit " << o.maxOver
            << ") maxd=" << diff.maxDelta << " (limit " << o.maxDelta << ") "
            << (ok ? "ok" : "MISMATCH") << "\n";
  return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  Options o;
  const std::string cmd = argc > 1 ? argv[1] : "";
  if ((cmd != "blank" && cmd != "compare") || !parse(argc, argv, &o)) {
    std::cerr << "usage: glim-golden blank IMAGE [--crop x,y,w,h]\n"
              << "       glim-golden compare GOT GOLDEN [--crop x,y,w,h] [--tolerance N]\n"
              << "                           --max-over N --max-delta N [--save FILE]\n";
    return 2;
  }
  return cmd == "blank" ? blank(o) : compare(o);
}
