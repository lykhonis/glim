#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace glim::shell {

class RunLoop final {
public:
    struct TaskContext {
        void cancel() { cancelled_ = true; }
        void stop() { stopped_ = true; }

    private:
        friend class RunLoop;
        bool cancelled_ = false;
        bool stopped_ = false;
    };

    using Task = std::function<void(TaskContext&)>;
    using FrameCallback = std::function<void()>;

    RunLoop();

    void run();
    void wake();
    void stop();
    void scheduleTimer(std::chrono::duration<float>);
    void scheduleTask(Task);
    void scheduleRepeatedTask(Task);
    void scheduleFrameCallback(FrameCallback);
    void requestFrame();

private:
    struct ScheduledTask {
        void operator()(TaskContext& context) const { task(context); }
        Task task;
        bool repeated = false;
    };

    struct LocalRunLoop {
        static LocalRunLoop& get();
        LocalRunLoop() noexcept;
        ~LocalRunLoop();

        int wakeFd = -1;
        int timerFd = -1;
        std::atomic_bool running{false};
        std::atomic_bool framePending{false};
        std::mutex mutex;
        std::vector<ScheduledTask> tasks;
        FrameCallback frameCallback;
    };

    LocalRunLoop& local_;
};

}  // namespace glim::shell
