#pragma once

namespace glim::shell {

class Application final {
public:
    Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application();

    void run();
};

}  // namespace glim::shell
