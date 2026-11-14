#include <glim/trace.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main() {
  const std::string path = "glim-trace-test.json";
  if (glim::trace::startFile("/nonexistent-dir/x.json")) {
    std::cerr << "FAIL: unwritable path accepted\n";
    return EXIT_FAILURE;
  }
  setenv("GLIM_TRACE", path.c_str(), 1);
  if (!glim::trace::enabled() || glim::trace::startFile(path.c_str())) {
    std::cerr << "FAIL: GLIM_TRACE starts the file once; second start refused\n";
    return EXIT_FAILURE;
  }
  glim::trace::setEnabled(true);
  {
    glim::trace::Span frame("frame");
    glim::trace::Span build("build");
  }
  glim::trace::counter("gpu_ms", 1.5);
  glim::trace::setEnabled(false);
  glim::trace::stopFile();
  glim::trace::stopFile();

  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string json = ss.str();
  std::remove(path.c_str());
  const bool ok = json.rfind("{\"traceEvents\":[", 0) == 0 &&
                  json.find("\"name\":\"frame\",\"ph\":\"X\"") != std::string::npos &&
                  json.find("\"name\":\"build\",\"ph\":\"X\"") != std::string::npos &&
                  json.find("\"name\":\"gpu_ms\",\"ph\":\"C\"") != std::string::npos &&
                  json.find("\"value\":1.5") != std::string::npos &&
                  json.find("\"displayTimeUnit\"") != std::string::npos;
  if (!ok) {
    std::cerr << "FAIL: trace json\n" << json;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
