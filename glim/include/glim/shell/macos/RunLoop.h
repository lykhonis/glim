#pragma once

#include <functional>
#include <chrono>
#include <vector>
#include <mutex>
#include <atomic>

#include <CoreFoundation/CoreFoundation.h>

namespace glim::shell::macos {
    class RunLoop final {
    public:
        struct TaskContext {
            void cancel() {
                cancelled_ = true;
            }

            void stop() {
                stopped_ = true;
            }

        private:
            friend class RunLoop;

            bool cancelled_ = false;
            bool stopped_ = false;
        };

        typedef std::function<void(TaskContext &)> Task;

        RunLoop();

        void run();

        void wake();

        void stop();

        void scheduleTimer(std::chrono::duration<float>);

        void scheduleTask(Task);

        void scheduleRepeatedTask(Task);

    private:
        struct ScheduledTask {
            void operator()(TaskContext &context) const {
                task(context);
            }

            Task task;
            bool repeated = false;
        };

        struct LocalRunLoop {
            static LocalRunLoop &get();

            LocalRunLoop() noexcept;

            ~LocalRunLoop();

            CFRunLoopRef runLoop;
            CFRunLoopTimerRef timer;
            std::atomic_bool running;
            std::mutex mutex;
            std::vector<ScheduledTask> tasks;
        };

        LocalRunLoop &local_;
    };
}
