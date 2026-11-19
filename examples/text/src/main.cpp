#include <glim/toolkit.h>
#include <glim/toolkit/host/Host.h>

#include <iostream>
#include <string>

using namespace glim::toolkit;
namespace host = glim::toolkit::host;

// One text field at the window's top left. Each new value is logged,
// so a device test can type into it and read what the field holds.
#if defined(__ANDROID__)
extern "C" int glim_app_main(int, char**) {
#else
int main(int, char**) {
#endif
  std::string logged;
  return host::run(
    {.title = "Glim Text", .width = 480, .height = 320},
    [&](const Engine& e) {
      State<TextEdit> value = rootScope(e).state("value", TextEdit{});
      if (value->text != logged) {
        logged = value->text;
        std::cout << "text value " << logged << std::endl;
      }
      return toBlock(textField(
        {.value = value, .width = 320, .placeholder = "Type here"}
      ));
    }
  );
}
