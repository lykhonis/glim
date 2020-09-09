#include <glim/shell/macos/RunLoop.h>

extern "C" void timerCallback(CFRunLoopTimerRef, void *info) {
    CFRunLoopStop(static_cast<CFRunLoopRef>(info));
}

namespace glim::shell::macos {
    static const CFTimeInterval DISTANT_FUTURE = 1.0e10;

    RunLoop::LocalRunLoop &RunLoop::LocalRunLoop::get() {
        thread_local LocalRunLoop runLoop;
        return runLoop;
    }

    RunLoop::LocalRunLoop::LocalRunLoop() noexcept
            : runLoop(CFRunLoopGetCurrent()) {
        auto timerContext = CFRunLoopTimerContext{
                .version = 0,
                .info = runLoop,
                .retain = nullptr,
                .release = nullptr,
                .copyDescription = nullptr,
        };
        timer = CFRunLoopTimerCreate(
                kCFAllocatorDefault,
                DISTANT_FUTURE,
                DISTANT_FUTURE,
                0,
                0,
                timerCallback,
                &timerContext);
        CFRunLoopAddTimer(runLoop, timer, kCFRunLoopCommonModes);
    }

    RunLoop::LocalRunLoop::~LocalRunLoop() {
        CFRunLoopRemoveTimer(runLoop, timer, kCFRunLoopCommonModes);
        CFRelease(timer);
    }

    RunLoop::RunLoop() : local_(LocalRunLoop::get()) {
    }

    void RunLoop::run() {
        local_.running = true;
        while (local_.running.load(std::memory_order_relaxed)) {
            auto result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, DISTANT_FUTURE, true);
            if (result == kCFRunLoopRunFinished) {
                break;
            }
            {
                std::vector<ScheduledTask> tasks;
                std::vector<ScheduledTask> repeatedTasks;
                {
                    std::scoped_lock lock(local_.mutex);
                    std::move(local_.tasks.begin(), local_.tasks.end(), std::back_inserter(tasks));
                    local_.tasks.clear();
                }
                for (auto &task : tasks) {
                    TaskContext context;
                    task(context);
                    if (context.stopped_) {
                        local_.running = false;
                    }
                    if (task.repeated && !context.cancelled_) {
                        repeatedTasks.push_back(std::move(task));
                    }
                }
                {
                    std::scoped_lock lock(local_.mutex);
                    std::move(repeatedTasks.begin(), repeatedTasks.end(), std::back_inserter(local_.tasks));
                }
            }
        }
    }

    void RunLoop::wake() {
        CFRunLoopStop(local_.runLoop);
    }

    void RunLoop::stop() {
        local_.running = false;
        CFRunLoopStop(local_.runLoop);
    }

    void RunLoop::scheduleTimer(std::chrono::duration<float> delay) {
        auto time = CFAbsoluteTimeGetCurrent() + delay.count();
        CFRunLoopTimerSetNextFireDate(local_.timer, time);
    }

    void RunLoop::scheduleTask(Task task) {
        std::scoped_lock lock(local_.mutex);
        local_.tasks.push_back({std::move(task)});
    }

    void RunLoop::scheduleRepeatedTask(Task task) {
        std::scoped_lock lock(local_.mutex);
        local_.tasks.push_back({std::move(task), true});
    }
}
