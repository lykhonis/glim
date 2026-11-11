#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Factories.h"

namespace fs = std::filesystem;

namespace {

// Factory scope: element factories, example widget dirs, themed().
constexpr const char* kScopeDirs[] = {
    "packages/glim-toolkit/include/glim/toolkit/elements",
    "examples/smarthome/src/widgets",
    "examples/infotainment/src/widgets",
};
constexpr const char* kScopeFiles[] = {
    "packages/glim-toolkit/include/glim/toolkit/theme/Themed.h",
};

std::string slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read " + p.string());
  std::ostringstream os;
  os << in.rdbuf();
  return os.str();
}

std::vector<fs::path> scopedFiles(const fs::path& root) {
  std::vector<fs::path> files;
  for (const char* d : kScopeDirs) {
    std::vector<fs::path> dir;
    for (const auto& e : fs::directory_iterator(root / d))
      if (e.path().extension() == ".h") dir.push_back(e.path());
    std::sort(dir.begin(), dir.end());
    files.insert(files.end(), dir.begin(), dir.end());
  }
  for (const char* f : kScopeFiles) files.push_back(root / f);
  return files;
}

}  // namespace

// glim-lint [--root DIR] [FILE...]
// No files: lint the repo's factory scope under DIR (default ".").
int main(int argc, char** argv) {
  try {
    fs::path root = ".";
    std::vector<fs::path> files;
    for (int i = 1; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "--root" && i + 1 < argc)
        root = argv[++i];
      else
        files.emplace_back(a);
    }
    if (files.empty()) files = scopedFiles(root);

    int factories = 0;
    int bad = 0;
    for (const fs::path& f : files) {
      glimlint::FactoryReport r = glimlint::checkFactories(f.string(), slurp(f));
      factories += r.factories;
      for (const glimlint::Finding& x : r.findings) {
        std::cout << x.file << ":" << x.line << ": " << x.message << "\n";
        ++bad;
      }
    }
    if (bad) return 1;
    std::cout << "factories OK: " << factories << " checked\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "glim-lint: " << e.what() << "\n";
    return 2;
  }
}
