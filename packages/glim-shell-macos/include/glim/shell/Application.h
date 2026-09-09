#pragma once

namespace glim::shell {

class Application final {
public:
    Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application();

    void run();

private:
    void* delegate_ = nullptr;
    void* application_ = nullptr;
};

}  // namespace glim::shell
